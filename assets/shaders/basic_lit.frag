#version 330 core

in vec3 fragNormal;
in vec3 fragWorldPosition;
in vec2 fragUv;

uniform vec4 baseColor;
uniform sampler2D baseColorTexture;
uniform bool useTexture;
uniform vec3 lightDirection;
uniform vec3 ambientColor;
uniform bool useLighting;

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

    if (useLighting)
    {
        lighting += vec3(diffuse);
    }
    else
    {
        lighting = vec3(1.0);
    }

    outColor = vec4(surfaceColor.rgb * lighting, surfaceColor.a);
}
