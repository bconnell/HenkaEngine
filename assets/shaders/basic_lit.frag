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
uniform vec3 lightDirection;
uniform vec3 ambientColor;
uniform bool useLighting;
uniform sampler2D normalTexture;
uniform sampler2D metallicRoughnessTexture;
uniform sampler2D emissiveTexture;
uniform bool useNormalTexture;
uniform bool useMetallicRoughnessTexture;
uniform bool useEmissiveTexture;
uniform float metallic;
uniform float roughness;
uniform float normalScale;
uniform vec3 emissiveColor;
uniform float emissiveStrength;
uniform int alphaMode;
uniform float alphaCutoff;
uniform sampler2D shadowMap;
uniform bool useShadowMap;

out vec4 outColor;

void main()
{
    vec3 normal = normalize(fragNormal);
    vec3 lightDir = normalize(-lightDirection);
    float diffuse = max(dot(normal, lightDir), 0.0);
    vec3 lighting = ambientColor;
    vec4 surfaceColor = baseColor;

    if (useTexture)
    {
        surfaceColor *= texture(baseColorTexture, fragUv);
    }
    if (alphaMode == 1 && surfaceColor.a < alphaCutoff)
    {
        discard;
    }

    float surfaceMetallic = metallic;
    float surfaceRoughness = roughness;
    if (useMetallicRoughnessTexture)
    {
        vec4 materialData = texture(metallicRoughnessTexture, fragUv);
        surfaceMetallic *= materialData.b;
        surfaceRoughness *= materialData.g;
    }
    if (useNormalTexture)
    {
        vec3 tangent = normalize(fragTangent - normal * dot(normal, fragTangent));
        vec3 bitangent = normalize(cross(normal, tangent)) * fragTangentHandedness;
        mat3 tangentFrame = mat3(tangent, bitangent, normal);
        vec3 mappedNormal = texture(normalTexture, fragUv).xyz * 2.0 - 1.0;
        mappedNormal.xy *= normalScale;
        normal = normalize(tangentFrame * normalize(mappedNormal));
    }

    if (useLighting)
    {
        float shadow = 1.0;
        if (useShadowMap)
        {
            vec3 shadowCoordinate = fragLightSpacePosition.xyz / max(fragLightSpacePosition.w, 0.0001);
            shadowCoordinate = shadowCoordinate * 0.5 + 0.5;
            if (shadowCoordinate.x >= 0.0 && shadowCoordinate.x <= 1.0 && shadowCoordinate.y >= 0.0 && shadowCoordinate.y <= 1.0)
            {
                float currentDepth = shadowCoordinate.z - 0.0015;
                vec2 texelSize = 1.0 / vec2(textureSize(shadowMap, 0));
                for (int x = -1; x <= 1; ++x)
                {
                    for (int y = -1; y <= 1; ++y)
                    {
                        shadow += texture(shadowMap, shadowCoordinate.xy + vec2(x, y) * texelSize).r >= currentDepth ? 1.0 : 0.0;
                    }
                }
                shadow /= 10.0;
            }
        }
        float highlight = pow(max(dot(reflect(-lightDir, normal), normalize(-fragWorldPosition)), 0.0),
            max(2.0, (1.0 - surfaceRoughness) * 64.0));
        lighting += shadow * (vec3(diffuse) * (1.0 - surfaceMetallic * 0.65) + vec3(highlight) * surfaceMetallic);
    }
    else
    {
        lighting = vec3(1.0);
    }

    vec3 emissive = emissiveColor * emissiveStrength;
    if (useEmissiveTexture)
    {
        emissive *= texture(emissiveTexture, fragUv).rgb;
    }
    outColor = vec4(surfaceColor.rgb * lighting + emissive, surfaceColor.a);
}
