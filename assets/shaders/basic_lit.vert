#version 330 core

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUv;
layout (location = 3) in vec4 inTangent;
layout (location = 4) in vec4 inColor;
layout (location = 5) in vec2 inUv1;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 lightMatrix;
uniform mat4 cascadeShadowMatrix;
uniform mat4 localShadowMatrix;
uniform mat4 previousViewProjection;

out vec3 fragNormal;
out vec3 fragWorldPosition;
out vec2 fragUv;
out vec2 fragUv1;
out vec3 fragTangent;
out float fragTangentHandedness;
out vec4 fragVertexColor;
out vec4 fragLightSpacePosition;
out vec4 fragCascadeShadowPosition;
out vec4 fragLocalShadowPosition;
out vec4 fragCurrentClipPosition;
out vec4 fragPreviousClipPosition;

void main()
{
    vec4 worldPosition = model * vec4(inPosition, 1.0);
    fragWorldPosition = worldPosition.xyz;
    mat3 normalMatrix = mat3(transpose(inverse(model)));
    fragNormal = normalMatrix * inNormal;
    fragUv = inUv;
    fragUv1 = inUv1;
    fragTangent = mat3(model) * inTangent.xyz;
    fragTangentHandedness = inTangent.w * (determinant(mat3(model)) < 0.0 ? -1.0 : 1.0);
    fragVertexColor = inColor;
    fragLightSpacePosition = lightMatrix * worldPosition;
    fragCascadeShadowPosition = cascadeShadowMatrix * worldPosition;
    fragLocalShadowPosition = localShadowMatrix * worldPosition;
    fragCurrentClipPosition = projection * view * worldPosition;
    fragPreviousClipPosition = previousViewProjection * worldPosition;
    gl_Position = fragCurrentClipPosition;
}
