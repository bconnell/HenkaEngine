#version 330 core

in vec3 fragNormal;
in vec3 fragWorldPosition;
in vec2 fragUv;
in vec2 fragUv1;
in vec3 fragTangent;
in float fragTangentHandedness;
in vec4 fragVertexColor;
in vec4 fragTerrainWeights;
in vec4 fragLightSpacePosition;
in vec4 fragCascadeShadowPosition;
in vec4 fragLocalShadowPosition;
in vec4 fragCurrentClipPosition;
in vec4 fragPreviousClipPosition;

uniform vec4 baseColor;
uniform sampler2D baseColorTexture;
uniform int baseColorUvSet;
uniform bool useTexture;
uniform bool useVertexColor;
uniform bool useTerrainLayers;
uniform vec4 terrainLayerBaseColor[4];
uniform vec4 terrainLayerParameters[4];
uniform int terrainLayerBaseColorAvailable[4];
uniform int terrainLayerNormalAvailable[4];
uniform int terrainLayerMetallicRoughnessAvailable[4];
uniform sampler2D terrainLayerBaseColorTextures[4];
uniform sampler2D terrainLayerNormalTextures[4];
uniform sampler2D terrainLayerMetallicRoughnessTextures[4];
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
uniform int environmentMode;
uniform vec3 environmentSunDirection;
uniform vec3 environmentSunColor;
uniform float environmentSunIntensity;
uniform vec4 environmentAtmosphere;
uniform vec3 environmentGroundAlbedo;
uniform float environmentHorizonIntensity;
uniform float environmentSunAngularRadius;
uniform vec3 environmentMoonDirection;
uniform vec3 environmentMoonColor;
uniform float environmentMoonIntensity;
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
uniform bool useReflectionProbeBoxProjection;
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
uniform sampler2D thicknessTexture;
uniform int thicknessUvSet;
uniform bool useThicknessTexture;
uniform bool useNormalTexture;
uniform bool useMetallicRoughnessTexture;
uniform bool useOcclusionTexture;
uniform bool useEmissiveTexture;
uniform float metallic;
uniform float roughness;
uniform float specularFactor;
uniform vec3 specularColor;
uniform float ior;
uniform float transmission;
uniform float subsurface;
uniform float thickness;
uniform float attenuationDistance;
uniform vec3 attenuationColor;
uniform vec3 subsurfaceColor;
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
uniform sampler2D cascadeShadowMap;
uniform bool useCascadeShadowMap;
uniform float cascadeSplitDistance;
uniform float cascadeBlendDistance;
uniform samplerCube pointShadowMap;
uniform bool usePointShadowMap;
uniform vec3 pointShadowLightPosition;
uniform float pointShadowFarPlane;
uniform sampler2D localShadowMap;
uniform bool useLocalShadowMap;
uniform mat4 localShadowMatrix;
uniform bool useMotionVectors;

out vec4 outColor;
layout(location = 1) out vec2 outMotion;
layout(location = 2) out float outReactive;

const float PI = 3.14159265359;

float saturate(float value)
{
    return clamp(value, 0.0, 1.0);
}

vec2 materialUv(int uvSet)
{
    return uvSet == 1 ? fragUv1 : fragUv;
}

float terrainMacroHash(vec2 cell)
{
    return fract(sin(dot(cell, vec2(127.1, 311.7))) * 43758.5453);
}

float terrainMacroVariation(vec2 worldPosition)
{
    vec2 cellPosition = worldPosition * 0.035;
    vec2 cell = floor(cellPosition);
    vec2 fraction = fract(cellPosition);
    fraction = fraction * fraction * (3.0 - 2.0 * fraction);
    float lower = mix(
        terrainMacroHash(cell),
        terrainMacroHash(cell + vec2(1.0, 0.0)),
        fraction.x);
    float upper = mix(
        terrainMacroHash(cell + vec2(0.0, 1.0)),
        terrainMacroHash(cell + vec2(1.0, 1.0)),
        fraction.x);
    return mix(lower, upper, fraction.y);
}

/* Terrain layer images are intentionally bounded runtime fixtures in the
 * Sandbox, so their authored resolution must not be the only source of
 * material detail. This value-noise path is shared by albedo, roughness, and
 * tangent-normal response. It is deliberately subtle and world-space so it
 * breaks large texture repeats without producing a painted overlay or a
 * camera-dependent pattern. */
float terrainDetailHash(vec2 cell)
{
    return fract(sin(dot(cell, vec2(127.1, 311.7)) + 19.19) * 43758.5453);
}

float terrainDetailNoise(vec2 position)
{
    vec2 cell = floor(position);
    vec2 fraction = fract(position);
    fraction = fraction * fraction * (3.0 - 2.0 * fraction);
    float lower = mix(
        terrainDetailHash(cell),
        terrainDetailHash(cell + vec2(1.0, 0.0)),
        fraction.x);
    float upper = mix(
        terrainDetailHash(cell + vec2(0.0, 1.0)),
        terrainDetailHash(cell + vec2(1.0, 1.0)),
        fraction.x);
    return mix(lower, upper, fraction.y);
}

float terrainLayerDetail(vec2 worldPosition, float layerIndex)
{
    vec2 offset = vec2(layerIndex * 17.0 + 3.0, layerIndex * 11.0 + 5.0);
    float fine = terrainDetailNoise(worldPosition * 0.34 + offset);
    float medium = terrainDetailNoise(worldPosition * 0.12 + offset * 0.47);
    return (fine - 0.5) * 0.78 + (medium - 0.5) * 0.34;
}

vec2 terrainLayerDetailSlope(vec2 worldPosition, float layerIndex)
{
    const float sampleDistance = 0.28;
    float xForward = terrainLayerDetail(worldPosition + vec2(sampleDistance, 0.0), layerIndex);
    float xBackward = terrainLayerDetail(worldPosition - vec2(sampleDistance, 0.0), layerIndex);
    float zForward = terrainLayerDetail(worldPosition + vec2(0.0, sampleDistance), layerIndex);
    float zBackward = terrainLayerDetail(worldPosition - vec2(0.0, sampleDistance), layerIndex);
    return vec2(xForward - xBackward, zForward - zBackward) /
        (2.0 * sampleDistance);
}

vec3 safeNormalize(vec3 value, vec3 fallback)
{
    float lengthSquared = dot(value, value);
    return lengthSquared > 0.000001 ? value * inversesqrt(lengthSquared) : fallback;
}

/* A bounded three-lobe diffusion-profile approximation for direct light. The
 * lobes widen with authored thickness, which gives wax, skin, and thin
 * translucent materials a more useful response than a single wrapped term.
 * It is intentionally local to the shaded point: it does not claim true
 * screen-space or multi-scatter transport. */
float subsurfaceDirectProfile(vec3 normal, vec3 lightDirection, float thicknessValue)
{
    float normalLight = dot(normal, lightDirection);
    float backLight = saturate(-normalLight);
    float broadWidth = mix(0.20, 0.62, thicknessValue);
    float broadLobe = saturate((normalLight + broadWidth) / (1.0 + broadWidth));
    float wideLobe = pow(backLight, mix(5.0, 1.80, thicknessValue));
    float middleLobe = pow(backLight, mix(11.0, 3.50, thicknessValue));
    float narrowLobe = pow(backLight, mix(22.0, 7.0, thicknessValue));
    return broadLobe * 0.24 + wideLobe * 0.46 + middleLobe * 0.22 + narrowLobe * 0.08;
}

vec3 parallaxCorrectReflectionDirection(vec3 direction)
{
    if (!useReflectionProbe || !useReflectionProbeBoxProjection)
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

float nearCascadeShadowFactor(vec3 normal, vec3 lightDir)
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

float farCascadeShadowFactor(vec3 normal, vec3 lightDir)
{
    if (!useCascadeShadowMap)
    {
        return 1.0;
    }
    float lightSpaceW = fragCascadeShadowPosition.w;
    if (abs(lightSpaceW) <= 0.0001)
    {
        return 1.0;
    }
    vec3 shadowCoordinate = fragCascadeShadowPosition.xyz / lightSpaceW;
    shadowCoordinate = shadowCoordinate * 0.5 + 0.5;
    if (shadowCoordinate.x < 0.0 || shadowCoordinate.x > 1.0 ||
        shadowCoordinate.y < 0.0 || shadowCoordinate.y > 1.0 ||
        shadowCoordinate.z < 0.0 || shadowCoordinate.z > 1.0)
    {
        return 1.0;
    }
    float nDotL = saturate(dot(normal, lightDir));
    float currentDepth = shadowCoordinate.z - max(0.0007 * (1.0 - nDotL), 0.0003);
    vec2 texelSize = 1.0 / vec2(textureSize(cascadeShadowMap, 0));
    float visible = 0.0;
    for (int x = -1; x <= 1; ++x)
    {
        for (int y = -1; y <= 1; ++y)
        {
            float sampleDepth = texture(cascadeShadowMap, shadowCoordinate.xy + vec2(x, y) * texelSize).r;
            visible += sampleDepth >= currentDepth ? 1.0 : 0.0;
        }
    }
    return visible / 9.0;
}

float shadowFactor(vec3 normal, vec3 lightDir)
{
    float cameraDistance = distance(fragWorldPosition, cameraPosition);
    float blendDistance = max(cascadeBlendDistance, 0.0);
    float blendStart = max(0.0, cascadeSplitDistance - blendDistance);
    if (useCascadeShadowMap && cameraDistance > blendStart)
    {
        float nearVisibility = nearCascadeShadowFactor(normal, lightDir);
        float farVisibility = farCascadeShadowFactor(normal, lightDir);
        float blend = smoothstep(blendStart, cascadeSplitDistance + blendDistance, cameraDistance);
        return mix(nearVisibility, farVisibility, blend);
    }
    return nearCascadeShadowFactor(normal, lightDir);
}

float pointShadowFactor(vec3 normal, vec3 lightDir)
{
    if (!usePointShadowMap)
    {
        return 1.0;
    }
    vec3 direction = fragWorldPosition - pointShadowLightPosition;
    float distanceToLight = length(direction);
    if (distanceToLight <= 0.0001 || distanceToLight > pointShadowFarPlane)
    {
        return 1.0;
    }
    float currentDepth = distanceToLight / max(pointShadowFarPlane, 0.0001) -
        max(0.002 * (1.0 - saturate(dot(normal, lightDir))), 0.0005);
    vec3 sampleDirection = direction / distanceToLight;
    vec3 offsets[4] = vec3[4](vec3(0.0), vec3(0.02, 0.0, 0.0), vec3(0.0, 0.02, 0.0), vec3(0.0, 0.0, 0.02));
    float visible = 0.0;
    for (int i = 0; i < 4; ++i)
    {
        visible += texture(pointShadowMap, normalize(sampleDirection + offsets[i])).r >= currentDepth ? 1.0 : 0.0;
    }
    return visible / 4.0;
}

float localShadowFactor(vec3 normal, vec3 lightDir)
{
    if (!useLocalShadowMap)
    {
        return 1.0;
    }
    float lightSpaceW = fragLocalShadowPosition.w;
    if (abs(lightSpaceW) <= 0.0001)
    {
        return 1.0;
    }
    vec3 shadowCoordinate = fragLocalShadowPosition.xyz / lightSpaceW;
    shadowCoordinate = shadowCoordinate * 0.5 + 0.5;
    if (shadowCoordinate.x < 0.0 || shadowCoordinate.x > 1.0 ||
        shadowCoordinate.y < 0.0 || shadowCoordinate.y > 1.0 ||
        shadowCoordinate.z < 0.0 || shadowCoordinate.z > 1.0)
    {
        return 1.0;
    }
    float nDotL = saturate(dot(normal, lightDir));
    float currentDepth = shadowCoordinate.z - max(0.0015 * (1.0 - nDotL), 0.0005);
    vec2 texelSize = 1.0 / vec2(textureSize(localShadowMap, 0));
    float visible = 0.0;
    for (int x = -1; x <= 1; ++x)
    {
        for (int y = -1; y <= 1; ++y)
        {
            float sampleDepth = texture(localShadowMap, shadowCoordinate.xy + vec2(x, y) * texelSize).r;
            visible += sampleDepth >= currentDepth ? 1.0 : 0.0;
        }
    }
    return visible / 9.0;
}

vec3 sampleProceduralEnvironment(vec3 direction)
{
    float up = clamp(direction.y, -1.0, 1.0);
    float daylight = max(up, 0.0);
    float rayleigh = clamp(environmentAtmosphere.x * max(environmentAtmosphere.z, 0.01), 0.0, 8.0);
    float mie = clamp(environmentAtmosphere.y * (0.65 + 0.35 * environmentAtmosphere.w), 0.0, 8.0);
    float horizonBand = exp(-abs(up) * 5.0) * max(environmentHorizonIntensity, 0.0);
    float horizon = smoothstep(0.04, 0.48, clamp(up * 0.5 + 0.5, 0.0, 1.0));
    vec3 lower = mix(environmentGroundColor, environmentHorizonColor, horizon);
    vec3 gradient = mix(lower, environmentZenithColor, smoothstep(0.48, 1.0, clamp(up * 0.5 + 0.5, 0.0, 1.0)));
    vec3 rayleighColor = vec3(0.24, 0.43, 0.95) * rayleigh * (0.16 + 0.84 * daylight);
    vec3 warmMie = vec3(1.0, 0.58, 0.30) * mie * pow(max(1.0 - daylight, 0.0), 1.35) * 0.12;
    vec3 sky = gradient + rayleighColor * (0.35 + 0.65 * daylight) + warmMie * horizonBand;
    sky = mix(sky, environmentGroundAlbedo, max(-up, 0.0) * 0.65);
    vec3 sunVector = length(environmentSunDirection) > 0.0001 ? normalize(-environmentSunDirection) : vec3(0.0, 1.0, 0.0);
    float sunFacing = max(dot(direction, sunVector), 0.0);
    float sunDisc = smoothstep(cos(max(environmentSunAngularRadius, 0.0001) * 2.0), 1.0, sunFacing);
    float sunHalo = pow(sunFacing, 32.0) * 0.035;
    return max(sky + (sunDisc + sunHalo) * max(environmentSunIntensity, 0.0) * max(environmentSunColor, vec3(0.0)), vec3(0.0));
}

vec3 sampleEnvironment(vec3 direction)
{
    if (environmentMode == 2)
    {
        return min(sampleProceduralEnvironment(direction) * max(environmentIntensity, 0.0), vec3(65504.0));
    }
    if (environmentMode == 1 && useEnvironmentTexture)
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
    if (useTerrainLayers)
    {
        vec4 terrainWeights = max(fragTerrainWeights, vec4(0.0));
        float terrainWeightSum = max(dot(terrainWeights, vec4(1.0)), 0.0001);
        terrainWeights /= terrainWeightSum;
        vec3 terrainAlbedo = vec3(0.0);
        for (int layerIndex = 0; layerIndex < 4; ++layerIndex)
        {
            vec4 layerColor = terrainLayerBaseColor[layerIndex];
            vec2 layerUv = fragWorldPosition.xz / max(terrainLayerParameters[layerIndex].z, 0.001);
            if (terrainLayerBaseColorAvailable[layerIndex] != 0)
            {
                layerColor *= texture(terrainLayerBaseColorTextures[layerIndex], layerUv);
            }
            float macro = terrainMacroVariation(fragWorldPosition.xz + vec2(float(layerIndex) * 19.0));
            float detail = terrainLayerDetail(fragWorldPosition.xz, float(layerIndex));
            layerColor.rgb *= clamp(0.90 + macro * 0.12 + detail * 0.18, 0.68, 1.14);
            terrainAlbedo += layerColor.rgb * terrainWeights[layerIndex];
        }
        surfaceColor = vec4(terrainAlbedo * max(baseColor.rgb, vec3(0.0)), baseColor.a);
    }
    else if (useTexture)
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
    if (useTerrainLayers)
    {
        vec4 terrainWeights = max(fragTerrainWeights, vec4(0.0));
        float terrainWeightSum = max(dot(terrainWeights, vec4(1.0)), 0.0001);
        terrainWeights /= terrainWeightSum;
        vec3 terrainTangentNormal = vec3(0.0, 0.0, 0.0);
        vec3 tangent = fragTangent - geometricNormal * dot(geometricNormal, fragTangent);
        tangent = safeNormalize(tangent, vec3(1.0, 0.0, 0.0));
        vec3 bitangent = safeNormalize(cross(geometricNormal, tangent), vec3(0.0, 0.0, 1.0)) * fragTangentHandedness;
        for (int layerIndex = 0; layerIndex < 4; ++layerIndex)
        {
            vec3 layerNormal = vec3(0.0, 0.0, 1.0);
            if (terrainLayerNormalAvailable[layerIndex] != 0)
            {
                vec2 layerUv = fragWorldPosition.xz / max(terrainLayerParameters[layerIndex].z, 0.001);
                layerNormal = texture(terrainLayerNormalTextures[layerIndex], layerUv).xyz * 2.0 - 1.0;
                layerNormal.xy *= terrainLayerParameters[layerIndex].w;
                layerNormal.xy += terrainLayerDetailSlope(
                    fragWorldPosition.xz, float(layerIndex)) *
                    (0.16 + terrainLayerParameters[layerIndex].w * 0.10);
                layerNormal = safeNormalize(layerNormal, vec3(0.0, 0.0, 1.0));
            }
            terrainTangentNormal += layerNormal * terrainWeights[layerIndex];
        }
        normal = safeNormalize(
            mat3(tangent, bitangent, geometricNormal) * safeNormalize(terrainTangentNormal, vec3(0.0, 0.0, 1.0)),
            geometricNormal);
    }
    else if (useNormalTexture)
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
    float surfaceTransmission = saturate(transmission);
    float surfaceSubsurface = saturate(subsurface);
    vec3 surfaceSubsurfaceColor = clamp(subsurfaceColor, vec3(0.0), vec3(1.0));
    float surfaceThickness = saturate(thickness);
    if (useThicknessTexture)
    {
        surfaceThickness *= clamp(texture(thicknessTexture, materialUv(thicknessUvSet)).r, 0.0, 1.0);
    }
    /* Reserve more diffuse energy as the authored thickness increases. The
     * response below remains a bounded raster approximation, not a profile or
     * thickness-texture diffusion model. */
    float diffuseEnergyWeight = 1.0 - surfaceSubsurface *
        mix(0.65, 0.78, surfaceThickness);
    float surfaceIor = clamp(ior, 1.01, 2.5);
    float safeAttenuationDistance = max(attenuationDistance, 0.0001);
    vec3 volumeTransmittance = pow(
        clamp(attenuationColor, vec3(0.0001), vec3(1.0)),
        vec3(surfaceThickness / safeAttenuationDistance));
    float surfaceClearcoat = saturate(clearcoat);
    float surfaceClearcoatRoughness = clamp(clearcoatRoughness, 0.045, 1.0);
    vec3 surfaceSheenColor = clamp(sheenColor, vec3(0.0), vec3(1.0));
    float surfaceSheenRoughness = clamp(sheenRoughness, 0.045, 1.0);
    if (useTerrainLayers)
    {
        vec4 terrainWeights = max(fragTerrainWeights, vec4(0.0));
        float terrainWeightSum = max(dot(terrainWeights, vec4(1.0)), 0.0001);
        terrainWeights /= terrainWeightSum;
        surfaceMetallic = 0.0;
        surfaceRoughness = 0.0;
        for (int layerIndex = 0; layerIndex < 4; ++layerIndex)
        {
            vec2 layerUv = fragWorldPosition.xz / max(terrainLayerParameters[layerIndex].z, 0.001);
            float layerMetallic = terrainLayerParameters[layerIndex].x;
            float layerRoughness = terrainLayerParameters[layerIndex].y;
            if (terrainLayerMetallicRoughnessAvailable[layerIndex] != 0)
            {
                vec4 layerMaterialData = texture(terrainLayerMetallicRoughnessTextures[layerIndex], layerUv);
                layerMetallic *= layerMaterialData.b;
                layerRoughness *= layerMaterialData.g;
            }
            float macro = terrainMacroVariation(fragWorldPosition.xz + vec2(float(layerIndex) * 19.0));
            float detail = terrainLayerDetail(fragWorldPosition.xz, float(layerIndex));
            layerRoughness *= clamp(0.94 + macro * 0.08 + detail * 0.10, 0.72, 1.10);
            surfaceMetallic += saturate(layerMetallic) * terrainWeights[layerIndex];
            surfaceRoughness += clamp(layerRoughness, 0.045, 1.0) * terrainWeights[layerIndex];
        }
        surfaceMetallic = saturate(surfaceMetallic);
        surfaceRoughness = clamp(surfaceRoughness, 0.045, 1.0);
    }
    else if (useMetallicRoughnessTexture)
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
    vec3 transmissionDirection = safeNormalize(
        refract(-viewDirection, normal, 1.0 / surfaceIor),
        -normal);
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
        vec3 diffuse = (1.0 - surfaceTransmission) * (1.0 - fresnel) * (1.0 - surfaceMetallic) * albedo * diffuseEnergyWeight / PI;
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
        color += albedo * surfaceSubsurfaceColor * surfaceSubsurface *
            subsurfaceDirectProfile(normal, lightDir, surfaceThickness) * radiance * shadow;

        /* The shared scene moon is a bounded second directional source. It is
         * intentionally shadowless in this slice: the directional sun shadow
         * contract remains authoritative, while the moon descriptor now
         * contributes consistently to Rendered PBR materials. */
        if (useEnvironment && environmentMoonIntensity > 0.0)
        {
            vec3 moonLightDir = safeNormalize(-environmentMoonDirection, vec3(0.0, 1.0, 0.0));
            vec3 moonRadiance = min(
                clamp(environmentMoonColor, vec3(0.0), vec3(16.0)) *
                    clamp(environmentMoonIntensity, 0.0, 16.0),
                vec3(65504.0));
            float moonNDotL = saturate(dot(normal, moonLightDir));
            vec3 moonHalfVector = safeNormalize(viewDirection + moonLightDir, normal);
            float moonNDotH = saturate(dot(normal, moonHalfVector));
            float moonVDotH = saturate(dot(viewDirection, moonHalfVector));
            vec3 moonFresnel = fresnelSchlick(moonVDotH, f0);
            float moonDistribution = distributionGGX(moonNDotH, alpha);
            float moonVisibility = visibilitySmithGGXCorrelated(nDotV, moonNDotL, alpha);
            vec3 moonSpecular = moonDistribution * moonVisibility * moonFresnel;
            vec3 moonDiffuse = (1.0 - surfaceTransmission) *
                (1.0 - moonFresnel) * (1.0 - surfaceMetallic) * albedo * diffuseEnergyWeight / PI;
            color += (moonDiffuse + moonSpecular) * baseLayerTransmission *
                moonRadiance * moonNDotL;
            float moonBackScatter = pow(saturate(dot(-normal, moonLightDir)), 2.0);
            float moonWrappedLight = saturate((dot(normal, moonLightDir) + 0.35) / 1.35);
            color += albedo * surfaceSubsurfaceColor * surfaceSubsurface *
                (moonBackScatter * 0.90 + moonWrappedLight * 0.10) * moonRadiance;
        }

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
        color += min(safeAmbient * ((1.0 - surfaceTransmission) * (1.0 - surfaceMetallic) * albedo * diffuseEnergyWeight + fresnel * 0.5) *
            baseLayerTransmission * occlusion, vec3(65504.0));
        if (useEnvironment)
        {
            vec3 environmentDiffuse = useIBL ? texture(iblIrradianceMap, normal).rgb : sampleEnvironment(normal);
            vec3 environmentBackScatter = useIBL ?
                texture(iblIrradianceMap, -normal).rgb : sampleEnvironment(-normal);
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
                environmentDiffuse * ((1.0 - surfaceTransmission) * (1.0 - surfaceMetallic) * albedo * diffuseEnergyWeight / PI) *
                    baseLayerTransmission * occlusion * 0.55 +
                environmentSpecular * (fresnel * brdf.x + brdf.y) * baseLayerTransmission * occlusion *
                    (0.35 + 0.65 * (1.0 - surfaceRoughness)) +
                environmentBackScatter * albedo * surfaceSubsurfaceColor * surfaceSubsurface *
                    (0.08 + 0.12 * surfaceThickness) * occlusion +
                sampleEnvironment(transmissionDirection) * albedo * surfaceTransmission * volumeTransmittance * (1.0 - fresnel) * 0.55,
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
            float localShadow = localLightOuterType[lightIndex].z > 1.5 ?
                pointShadowFactor(normal, localLightDirection) :
                (localLightOuterType[lightIndex].z > 0.5 ?
                    localShadowFactor(normal, localLightDirection) : 1.0);
            vec3 localSpecular = localDistribution * localVisibility * localFresnel;
            vec3 localDiffuse = (1.0 - surfaceTransmission) * (1.0 - localFresnel) * (1.0 - surfaceMetallic) * albedo * diffuseEnergyWeight / PI;
            color += (localDiffuse + localSpecular) * localRadiance * localNDotL * localShadow;
            color += albedo * surfaceSubsurfaceColor * surfaceSubsurface *
                subsurfaceDirectProfile(normal, localLightDirection, surfaceThickness) * localRadiance * localShadow;
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
    /* Reactive pixels must not drag stale history across transparency,
     * transmission, or rapidly changing emissive highlights. */
    outReactive = clamp(
        max(
            surfaceColor.a < 0.999 ? 1.0 : 0.0,
            max(surfaceTransmission * 0.65, max(emissive.r, max(emissive.g, emissive.b)) * 0.08)),
        0.0,
        1.0);
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
