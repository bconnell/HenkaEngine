#version 330 core

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUv;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec3 fragNormal;
out vec3 fragWorldPosition;

void main()
{
    vec4 worldPosition = model * vec4(inPosition, 1.0);
    fragWorldPosition = worldPosition.xyz;
    fragNormal = mat3(transpose(inverse(model))) * inNormal;
    gl_Position = projection * view * worldPosition;
}
