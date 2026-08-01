#version 330 core

in vec3 fragNormal;
in vec3 fragWorldPosition;
in vec2 fragUv;
in vec3 fragTangent;
in float fragTangentHandedness;
in vec4 fragLightSpacePosition;

uniform vec4 baseColor;
uniform sampler2D baseColorTexture;
uniform bool useTexture;
uniform vec3 cameraPosition;
uniform vec3 lightDirection;
uniform vec3 lightColor;
uniform float lightIntensity;
uniform vec3 ambientColor;
uniform bool useLighting;
uniform sampler2D normalTexture;
uniform sampler2D metallicRoughnessTexture;
uniform sampler2D occlusionTexture;
uniform sampler2D emissiveTexture;
uniform bool useNormalTexture;
uniform bool useMetallicRoughnessTexture;
uniform bool useOcclusionTexture;
uniform bool useEmissiveTexture;
uniform float metallic;
uniform float roughness;
uniform float normalScale;
uniform float occlusionStrength;
uniform vec3 emissiveColor;
uniform float emissiveStrength;
uniform int alphaMode;
uniform float alphaCutoff;
uniform sampler2D shadowMap;
uniform bool useShadowMap;

out vec4 outColor;

const float PI = 3.14159265359;

float saturate(float value)
{
    return clamp(value, 0.0, 1.0);
}

vec3 safeNormalize(vec3 value, vec3 fallback)
{
    float lengthSquared = dot(value, value);
    return lengthSquared > 0.000001 ? value * inversesqrt(lengthSquared) : fallback;
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
    float visible = 0.0;
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

void main()
{
    vec4 surfaceColor = baseColor;
    if (useTexture)
    {
        surfaceColor *= texture(baseColorTexture, fragUv);
    }
    if (alphaMode == 1 && surfaceColor.a < alphaCutoff)
    {
        discard;
    }

    vec3 geometricNormal = safeNormalize(fragNormal, vec3(0.0, 1.0, 0.0));
    vec3 normal = geometricNormal;
    if (useNormalTexture)
    {
        vec3 tangent = fragTangent - geometricNormal * dot(geometricNormal, fragTangent);
        tangent = safeNormalize(tangent, vec3(1.0, 0.0, 0.0));
        vec3 bitangent = safeNormalize(cross(geometricNormal, tangent), vec3(0.0, 0.0, 1.0)) * fragTangentHandedness;
        vec3 mappedNormal = texture(normalTexture, fragUv).xyz * 2.0 - 1.0;
        mappedNormal.xy *= normalScale;
        normal = safeNormalize(mat3(tangent, bitangent, geometricNormal) * safeNormalize(mappedNormal, vec3(0.0, 0.0, 1.0)), geometricNormal);
    }

    float surfaceMetallic = saturate(metallic);
    float surfaceRoughness = clamp(roughness, 0.045, 1.0);
    if (useMetallicRoughnessTexture)
    {
        vec4 materialData = texture(metallicRoughnessTexture, fragUv);
        surfaceMetallic = saturate(surfaceMetallic * materialData.b);
        surfaceRoughness = clamp(surfaceRoughness * materialData.g, 0.045, 1.0);
    }

    float occlusion = 1.0;
    if (useOcclusionTexture)
    {
        occlusion = mix(1.0, texture(occlusionTexture, fragUv).r, saturate(occlusionStrength));
    }

    vec3 albedo = max(surfaceColor.rgb, vec3(0.0));
    vec3 viewDirection = safeNormalize(cameraPosition - fragWorldPosition, vec3(0.0, 0.0, 1.0));
    vec3 lightDir = safeNormalize(-lightDirection, vec3(0.0, 1.0, 0.0));
    vec3 safeLightColor = clamp(lightColor, vec3(0.0), vec3(1.0));
    float safeLightIntensity = clamp(lightIntensity, 0.0, 10000.0);
    vec3 radiance = min(safeLightColor * safeLightIntensity, vec3(65504.0));
    vec3 f0 = mix(vec3(0.04), albedo, surfaceMetallic);
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
        float shadow = shadowFactor(normal, lightDir);
        color += (diffuse + specular) * radiance * nDotL * shadow;

        // Ambient is an indirect fallback until the environment/probe path is active.
        vec3 safeAmbient = min(max(ambientColor, vec3(0.0)), vec3(16.0));
        color += min(safeAmbient * ((1.0 - surfaceMetallic) * albedo + fresnel * 0.5) * occlusion, vec3(65504.0));
    }
    else
    {
        color = albedo;
    }

    vec3 emissive = min(max(emissiveColor, vec3(0.0)), vec3(1.0)) * clamp(emissiveStrength, 0.0, 100.0);
    if (useEmissiveTexture)
    {
        emissive *= max(texture(emissiveTexture, fragUv).rgb, vec3(0.0));
    }
    outColor = vec4(min(max(color + emissive, vec3(0.0)), vec3(65504.0)), surfaceColor.a);
}
