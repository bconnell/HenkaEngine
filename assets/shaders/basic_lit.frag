#version 330 core

in vec3 fragNormal;
in vec3 fragWorldPosition;
in vec2 fragUv;
in vec2 fragUv1;
in vec3 fragTangent;
in float fragTangentHandedness;
in vec4 fragVertexColor;
in vec4 fragLightSpacePosition;
in vec4 fragCurrentClipPosition;
in vec4 fragPreviousClipPosition;

uniform vec4 baseColor;
uniform sampler2D baseColorTexture;
uniform int baseColorUvSet;
uniform bool useTexture;
uniform bool useVertexColor;
uniform vec3 cameraPosition;
uniform vec3 lightDirection;
uniform vec3 lightColor;
uniform float lightIntensity;
uniform vec3 ambientColor;
uniform bool useLighting;
uniform bool useEnvironment;
uniform vec3 environmentGroundColor;
uniform vec3 environmentHorizonColor;
uniform vec3 environmentZenithColor;
uniform float environmentIntensity;
uniform sampler2D environmentTexture;
uniform bool useEnvironmentTexture;
uniform float environmentRotation;
uniform samplerCube iblIrradianceMap;
uniform samplerCube iblPrefilterMap;
uniform sampler2D iblBrdfLut;
uniform bool useIBL;
uniform vec3 reflectionProbePosition;
uniform vec3 reflectionProbeExtents;
uniform bool useReflectionProbe;
uniform samplerCube reflectionProbeMap;
uniform bool useReflectionProbeMap;
uniform bool doubleSided;
uniform int localLightCount;
uniform vec4 localLightPositionRange[4];
uniform vec4 localLightColorIntensity[4];
uniform vec4 localLightDirectionInner[4];
uniform vec4 localLightOuterType[4];
uniform bool fogEnabled;
uniform int fogMode;
uniform vec3 fogColor;
uniform float fogStartDistance;
uniform float fogEndDistance;
uniform float fogDensity;
uniform sampler2D normalTexture;
uniform int normalUvSet;
uniform sampler2D metallicRoughnessTexture;
uniform int metallicRoughnessUvSet;
uniform sampler2D occlusionTexture;
uniform int occlusionUvSet;
uniform sampler2D emissiveTexture;
uniform int emissiveUvSet;
uniform bool useNormalTexture;
uniform bool useMetallicRoughnessTexture;
uniform bool useOcclusionTexture;
uniform bool useEmissiveTexture;
uniform float metallic;
uniform float roughness;
uniform float specularFactor;
uniform vec3 specularColor;
uniform float ior;
uniform float normalScale;
uniform float occlusionStrength;
uniform vec3 emissiveColor;
uniform float emissiveStrength;
uniform float clearcoat;
uniform float clearcoatRoughness;
uniform vec3 sheenColor;
uniform float sheenRoughness;
uniform int alphaMode;
uniform float alphaCutoff;
uniform sampler2D shadowMap;
uniform bool useShadowMap;
uniform bool useMotionVectors;

out vec4 outColor;
layout(location = 1) out vec2 outMotion;

const float PI = 3.14159265359;

float saturate(float value)
{
    return clamp(value, 0.0, 1.0);
}

vec2 materialUv(int uvSet)
{
    return uvSet == 1 ? fragUv1 : fragUv;
}

vec3 safeNormalize(vec3 value, vec3 fallback)
{
    float lengthSquared = dot(value, value);
    return lengthSquared > 0.000001 ? value * inversesqrt(lengthSquared) : fallback;
}

vec3 parallaxCorrectReflectionDirection(vec3 direction)
{
    if (!useReflectionProbe)
    {
        return direction;
    }
    vec3 boxMin = reflectionProbePosition - reflectionProbeExtents;
    vec3 boxMax = reflectionProbePosition + reflectionProbeExtents;
    vec3 safeDirection = safeNormalize(direction, vec3(0.0, 0.0, 1.0));
    vec3 firstIntersection = (boxMax - fragWorldPosition) / safeDirection;
    vec3 secondIntersection = (boxMin - fragWorldPosition) / safeDirection;
    vec3 furthestIntersection = max(firstIntersection, secondIntersection);
    float distanceToBox = min(min(furthestIntersection.x, furthestIntersection.y), furthestIntersection.z);
    if (!(distanceToBox > 0.0) || distanceToBox > 65536.0)
    {
        return direction;
    }
    return safeNormalize(
        fragWorldPosition + safeDirection * distanceToBox - reflectionProbePosition,
        direction);
}

vec3 fresnelSchlick(float cosTheta, vec3 f0)
{
    return f0 + (1.0 - f0) * pow(1.0 - saturate(cosTheta), 5.0);
}

float distributionGGX(float nDotH, float alpha)
{
    float a2 = alpha * alpha;
    float denominator = nDotH * nDotH * (a2 - 1.0) + 1.0;
    denominator = max(PI * denominator * denominator, 0.0001);
    return a2 / denominator;
}

float visibilitySmithGGXCorrelated(float nDotV, float nDotL, float alpha)
{
    float a2 = alpha * alpha;
    float gv = nDotL * sqrt(max(nDotV * nDotV * (1.0 - a2) + a2, 0.0));
    float gl = nDotV * sqrt(max(nDotL * nDotL * (1.0 - a2) + a2, 0.0));
    return 0.5 / max(gv + gl, 0.0001);
}

float shadowFactor(vec3 normal, vec3 lightDir)
{
    if (!useShadowMap)
    {
        return 1.0;
    }

    float lightSpaceW = fragLightSpacePosition.w;
    if (abs(lightSpaceW) <= 0.0001)
    {
        return 1.0;
    }

    vec3 shadowCoordinate = fragLightSpacePosition.xyz / lightSpaceW;
    shadowCoordinate = shadowCoordinate * 0.5 + 0.5;
    if (shadowCoordinate.x < 0.0 || shadowCoordinate.x > 1.0 ||
        shadowCoordinate.y < 0.0 || shadowCoordinate.y > 1.0 ||
        shadowCoordinate.z < 0.0 || shadowCoordinate.z > 1.0)
    {
        return 1.0;
    }

    float nDotL = saturate(dot(normal, lightDir));
    float slopeBias = max(0.0005 * (1.0 - nDotL), 0.00025);
    float currentDepth = shadowCoordinate.z - slopeBias;
    vec2 texelSize = 1.0 / vec2(textureSize(shadowMap, 0));
    float centerDepth = texture(shadowMap, shadowCoordinate.xy).r;
    float centerVisible = centerDepth >= currentDepth ? 1.0 : 0.0;
    float visible = 0.0;
    if (centerVisible < 0.5)
    {
        /* A bounded wider kernel only near a blocker tightens contact edges
         * without paying the wider sample cost across the whole surface. */
        for (int x = -2; x <= 2; ++x)
        {
            for (int y = -2; y <= 2; ++y)
            {
                float sampleDepth = texture(shadowMap, shadowCoordinate.xy + vec2(x, y) * texelSize).r;
                visible += sampleDepth >= currentDepth ? 1.0 : 0.0;
            }
        }
        return min(visible / 25.0, 0.75);
    }
    for (int x = -1; x <= 1; ++x)
    {
        for (int y = -1; y <= 1; ++y)
        {
            float sampleDepth = texture(shadowMap, shadowCoordinate.xy + vec2(x, y) * texelSize).r;
            visible += sampleDepth >= currentDepth ? 1.0 : 0.0;
        }
    }
    return visible / 9.0;
}

vec3 sampleEnvironment(vec3 direction)
{
    if (useEnvironmentTexture)
    {
        float longitude = atan(direction.z, direction.x) / (2.0 * PI) + 0.5 + environmentRotation / (2.0 * PI);
        float latitude = acos(clamp(direction.y, -1.0, 1.0)) / PI;
        return min(max(texture(environmentTexture, vec2(fract(longitude), latitude)).rgb, vec3(0.0)) * max(environmentIntensity, 0.0), vec3(65504.0));
    }
    float height = clamp(direction.y * 0.5 + 0.5, 0.0, 1.0);
    float horizon = smoothstep(0.04, 0.48, height);
    vec3 lower = mix(environmentGroundColor, environmentHorizonColor, horizon);
    vec3 color = mix(lower, environmentZenithColor, smoothstep(0.48, 1.0, height));
    return min(max(color, vec3(0.0)) * max(environmentIntensity, 0.0), vec3(65504.0));
}

void main()
{
    vec4 surfaceColor = baseColor;
    if (useTexture)
    {
        surfaceColor *= texture(baseColorTexture, materialUv(baseColorUvSet));
    }
    if (useVertexColor)
    {
        surfaceColor *= fragVertexColor;
    }
    if (alphaMode == 1 && surfaceColor.a < alphaCutoff)
    {
        discard;
    }

    vec3 geometricNormal = safeNormalize(fragNormal, vec3(0.0, 1.0, 0.0));
    if (doubleSided && !gl_FrontFacing)
    {
        geometricNormal = -geometricNormal;
    }
    vec3 normal = geometricNormal;
    if (useNormalTexture)
    {
        vec3 tangent = fragTangent - geometricNormal * dot(geometricNormal, fragTangent);
        tangent = safeNormalize(tangent, vec3(1.0, 0.0, 0.0));
        vec3 bitangent = safeNormalize(cross(geometricNormal, tangent), vec3(0.0, 0.0, 1.0)) * fragTangentHandedness;
        vec3 mappedNormal = texture(normalTexture, materialUv(normalUvSet)).xyz * 2.0 - 1.0;
        mappedNormal.xy *= normalScale;
        normal = safeNormalize(mat3(tangent, bitangent, geometricNormal) * safeNormalize(mappedNormal, vec3(0.0, 0.0, 1.0)), geometricNormal);
    }

    float surfaceMetallic = saturate(metallic);
    float surfaceRoughness = clamp(roughness, 0.045, 1.0);
    float surfaceClearcoat = saturate(clearcoat);
    float surfaceClearcoatRoughness = clamp(clearcoatRoughness, 0.045, 1.0);
    vec3 surfaceSheenColor = clamp(sheenColor, vec3(0.0), vec3(1.0));
    float surfaceSheenRoughness = clamp(sheenRoughness, 0.045, 1.0);
    if (useMetallicRoughnessTexture)
    {
        vec4 materialData = texture(metallicRoughnessTexture, materialUv(metallicRoughnessUvSet));
        surfaceMetallic = saturate(surfaceMetallic * materialData.b);
        surfaceRoughness = clamp(surfaceRoughness * materialData.g, 0.045, 1.0);
    }

    float occlusion = 1.0;
    if (useOcclusionTexture)
    {
        occlusion = mix(1.0, texture(occlusionTexture, materialUv(occlusionUvSet)).r, saturate(occlusionStrength));
    }

    vec3 albedo = max(surfaceColor.rgb, vec3(0.0));
    vec3 viewDirection = safeNormalize(cameraPosition - fragWorldPosition, vec3(0.0, 0.0, 1.0));
    vec3 lightDir = safeNormalize(-lightDirection, vec3(0.0, 1.0, 0.0));
    vec3 safeLightColor = clamp(lightColor, vec3(0.0), vec3(1.0));
    float safeLightIntensity = clamp(lightIntensity, 0.0, 10000.0);
    vec3 radiance = min(safeLightColor * safeLightIntensity, vec3(65504.0));
    float dielectricF0 = pow((max(ior, 1.0) - 1.0) / (max(ior, 1.0) + 1.0), 2.0);
    vec3 f0 = mix(clamp(specularColor, vec3(0.0), vec3(1.0)) *
        dielectricF0 * saturate(specularFactor), albedo, surfaceMetallic);
    vec3 color = vec3(0.0);

    if (useLighting)
    {
        float nDotV = saturate(dot(normal, viewDirection));
        float nDotL = saturate(dot(normal, lightDir));
        vec3 halfVector = safeNormalize(viewDirection + lightDir, normal);
        float nDotH = saturate(dot(normal, halfVector));
        float vDotH = saturate(dot(viewDirection, halfVector));
        float alpha = surfaceRoughness * surfaceRoughness;
        vec3 fresnel = fresnelSchlick(vDotH, f0);
        float distribution = distributionGGX(nDotH, alpha);
        float visibility = visibilitySmithGGXCorrelated(nDotV, nDotL, alpha);
        vec3 specular = distribution * visibility * fresnel;
        vec3 diffuse = (1.0 - fresnel) * (1.0 - surfaceMetallic) * albedo / PI;
        vec3 baseLayerTransmission = vec3(1.0);
        if (surfaceClearcoat > 0.0)
        {
            vec3 clearcoatViewFresnel = fresnelSchlick(nDotV, vec3(0.04));
            baseLayerTransmission *= 1.0 - clearcoatViewFresnel * surfaceClearcoat;
        }
        if (max(max(surfaceSheenColor.r, surfaceSheenColor.g), surfaceSheenColor.b) > 0.0)
        {
            vec3 sheenTransmission = vec3(1.0) -
                surfaceSheenColor * (1.0 - surfaceMetallic) * 0.35;
            baseLayerTransmission *= max(sheenTransmission, vec3(0.0));
        }
        float shadow = shadowFactor(normal, lightDir);
        color += (diffuse + specular) * baseLayerTransmission * radiance * nDotL * shadow;

        if (surfaceClearcoat > 0.0)
        {
            float clearcoatAlpha = surfaceClearcoatRoughness * surfaceClearcoatRoughness;
            vec3 clearcoatFresnel = fresnelSchlick(vDotH, vec3(0.04));
            float clearcoatDistribution = distributionGGX(nDotH, clearcoatAlpha);
            float clearcoatVisibility = visibilitySmithGGXCorrelated(nDotV, nDotL, clearcoatAlpha);
            color += clearcoatFresnel * clearcoatDistribution * clearcoatVisibility *
                radiance * nDotL * shadow * surfaceClearcoat;
        }

        if (max(max(surfaceSheenColor.r, surfaceSheenColor.g), surfaceSheenColor.b) > 0.0)
        {
            float sheenAlpha = surfaceSheenRoughness * surfaceSheenRoughness;
            vec3 sheenFresnel = fresnelSchlick(vDotH, surfaceSheenColor);
            float sheenDistribution = distributionGGX(nDotH, sheenAlpha);
            float sheenVisibility = visibilitySmithGGXCorrelated(nDotV, nDotL, sheenAlpha);
            color += sheenFresnel * sheenDistribution * sheenVisibility *
                radiance * nDotL * shadow * (1.0 - surfaceMetallic) * 0.35;
        }

        // Ambient remains an indirect fallback for scenes without a richer probe path.
        vec3 safeAmbient = min(max(ambientColor, vec3(0.0)), vec3(16.0));
        color += min(safeAmbient * ((1.0 - surfaceMetallic) * albedo + fresnel * 0.5) *
            baseLayerTransmission * occlusion, vec3(65504.0));
        if (useEnvironment)
        {
            vec3 environmentDiffuse = useIBL ? texture(iblIrradianceMap, normal).rgb : sampleEnvironment(normal);
            vec3 reflectionDirection = reflect(-viewDirection, normal);
            vec3 blurredReflectionDirection = safeNormalize(
                mix(reflectionDirection, normal, surfaceRoughness * 0.75),
                normal);
            blurredReflectionDirection = parallaxCorrectReflectionDirection(blurredReflectionDirection);
            vec3 environmentSpecular = sampleEnvironment(blurredReflectionDirection);
            if (useIBL)
            {
                if (useReflectionProbeMap)
                {
                    environmentSpecular = textureLod(
                        reflectionProbeMap,
                        blurredReflectionDirection,
                        surfaceRoughness * 4.0).rgb;
                }
                else
                {
                    environmentSpecular = textureLod(
                        iblPrefilterMap,
                        blurredReflectionDirection,
                        surfaceRoughness * 4.0).rgb;
                }
            }
            vec2 brdf = useIBL ? texture(iblBrdfLut, vec2(nDotV, 1.0 - surfaceRoughness)).rg : vec2(1.0, 0.0);
            color += min(
                environmentDiffuse * ((1.0 - surfaceMetallic) * albedo / PI) *
                    baseLayerTransmission * occlusion * 0.55 +
                environmentSpecular * (fresnel * brdf.x + brdf.y) * baseLayerTransmission * occlusion *
                    (0.35 + 0.65 * (1.0 - surfaceRoughness)),
                vec3(65504.0));
        }

        for (int lightIndex = 0; lightIndex < 4; ++lightIndex)
        {
            if (lightIndex >= localLightCount)
            {
                break;
            }
            vec3 toLocalLight = localLightPositionRange[lightIndex].xyz - fragWorldPosition;
            float localDistance = length(toLocalLight);
            vec3 localLightDirection = safeNormalize(toLocalLight, normal);
            float localRange = max(localLightPositionRange[lightIndex].w, 0.0001);
            float rangeFade = saturate(1.0 - localDistance / localRange);
            float attenuation = rangeFade * rangeFade / max(localDistance * localDistance, 1.0);
            if (localLightOuterType[lightIndex].y > 0.5)
            {
                float cone = dot(-localLightDirection, safeNormalize(localLightDirectionInner[lightIndex].xyz, vec3(0.0, -1.0, 0.0)));
                attenuation *= smoothstep(
                    localLightOuterType[lightIndex].x,
                    localLightDirectionInner[lightIndex].w,
                    cone);
            }
            float localNDotL = saturate(dot(normal, localLightDirection));
            vec3 localHalf = safeNormalize(viewDirection + localLightDirection, normal);
            float localNDotH = saturate(dot(normal, localHalf));
            float localVDotH = saturate(dot(viewDirection, localHalf));
            vec3 localFresnel = fresnelSchlick(localVDotH, f0);
            float localDistribution = distributionGGX(localNDotH, alpha);
            float localVisibility = visibilitySmithGGXCorrelated(nDotV, localNDotL, alpha);
            vec3 localRadiance = min(
                clamp(localLightColorIntensity[lightIndex].rgb, vec3(0.0), vec3(16.0)) *
                clamp(localLightColorIntensity[lightIndex].w, 0.0, 100000.0) * attenuation,
                vec3(65504.0));
            vec3 localSpecular = localDistribution * localVisibility * localFresnel;
            vec3 localDiffuse = (1.0 - localFresnel) * (1.0 - surfaceMetallic) * albedo / PI;
            color += (localDiffuse + localSpecular) * localRadiance * localNDotL;
        }
    }
    else
    {
        color = albedo;
    }

    vec3 emissive = min(max(emissiveColor, vec3(0.0)), vec3(1.0)) * clamp(emissiveStrength, 0.0, 100.0);
    if (useEmissiveTexture)
    {
        emissive *= max(texture(emissiveTexture, materialUv(emissiveUvSet)).rgb, vec3(0.0));
    }
    vec3 finalColor = min(max(color + emissive, vec3(0.0)), vec3(65504.0));
    if (fogEnabled)
    {
        float distanceFromCamera = length(cameraPosition - fragWorldPosition);
        float fogAmount = 0.0;
        if (fogMode == 0)
        {
            fogAmount = clamp(
                (distanceFromCamera - max(fogStartDistance, 0.0)) /
                    max(fogEndDistance - fogStartDistance, 0.0001),
                0.0,
                1.0);
        }
        else if (fogMode == 1)
        {
            fogAmount = 1.0 - exp(-clamp(fogDensity, 0.0, 1.0) * distanceFromCamera);
        }
        else
        {
            float exponentialTerm = clamp(fogDensity, 0.0, 1.0) * distanceFromCamera;
            fogAmount = 1.0 - exp(-(exponentialTerm * exponentialTerm));
        }
        finalColor = mix(finalColor, max(fogColor, vec3(0.0)), fogAmount);
    }
    outColor = vec4(min(finalColor, vec3(65504.0)), surfaceColor.a);
    if (useMotionVectors && abs(fragCurrentClipPosition.w) > 0.0001 &&
        abs(fragPreviousClipPosition.w) > 0.0001)
    {
        vec2 currentNdc = fragCurrentClipPosition.xy / fragCurrentClipPosition.w;
        vec2 previousNdc = fragPreviousClipPosition.xy / fragPreviousClipPosition.w;
        outMotion = (currentNdc - previousNdc) * 0.5;
    }
    else
    {
        outMotion = vec2(0.0);
    }
}
