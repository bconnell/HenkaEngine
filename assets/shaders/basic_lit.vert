#version 330 core

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUv;
layout (location = 3) in vec4 inTangent;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 lightMatrix;

out vec3 fragNormal;
out vec3 fragWorldPosition;
out vec2 fragUv;
out vec3 fragTangent;
out float fragTangentHandedness;
out vec4 fragLightSpacePosition;

void main()
{
    vec4 worldPosition = model * vec4(inPosition, 1.0);
    fragWorldPosition = worldPosition.xyz;
    fragNormal = mat3(transpose(inverse(model))) * inNormal;
    fragUv = inUv;
    fragTangent = mat3(model) * inTangent.xyz;
    fragTangentHandedness = inTangent.w;
    fragLightSpacePosition = lightMatrix * worldPosition;
    gl_Position = projection * view * worldPosition;
}
