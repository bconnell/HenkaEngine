#version 330 core

in vec3 fragNormal;
in vec3 fragWorldPosition;

uniform vec4 baseColor;
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

    if (useLighting)
    {
        lighting += vec3(diffuse);
    }
    else
    {
        lighting = vec3(1.0);
    }

    outColor = vec4(baseColor.rgb * lighting, baseColor.a);
}
