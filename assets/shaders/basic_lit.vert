#version 330 core

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUv;
layout (location = 3) in vec4 inTangent;
layout (location = 4) in vec4 inColor;
layout (location = 5) in vec2 inUv1;
layout (location = 6) in vec4 instanceModel0;
layout (location = 7) in vec4 instanceModel1;
layout (location = 8) in vec4 instanceModel2;
layout (location = 9) in vec4 instanceModel3;
layout (location = 10) in vec4 instancePreviousModel0;
layout (location = 11) in vec4 instancePreviousModel1;
layout (location = 12) in vec4 instancePreviousModel2;
layout (location = 13) in vec4 instancePreviousModel3;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 lightMatrix;
uniform mat4 cascadeShadowMatrix;
uniform mat4 localShadowMatrix;
uniform mat4 previousViewProjection;
uniform mat4 previousModel;
uniform bool useInstancing;

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
    mat4 activeModel = useInstancing ?
        mat4(instanceModel0, instanceModel1, instanceModel2, instanceModel3) : model;
    mat4 activePreviousModel = useInstancing ?
        mat4(instancePreviousModel0, instancePreviousModel1, instancePreviousModel2, instancePreviousModel3) : previousModel;
    vec4 worldPosition = activeModel * vec4(inPosition, 1.0);
    fragWorldPosition = worldPosition.xyz;
    mat3 normalMatrix = mat3(transpose(inverse(activeModel)));
    fragNormal = normalMatrix * inNormal;
    fragUv = inUv;
    fragUv1 = inUv1;
    fragTangent = mat3(activeModel) * inTangent.xyz;
    fragTangentHandedness = inTangent.w * (determinant(mat3(activeModel)) < 0.0 ? -1.0 : 1.0);
    fragVertexColor = inColor;
    fragLightSpacePosition = lightMatrix * worldPosition;
    fragCascadeShadowPosition = cascadeShadowMatrix * worldPosition;
    fragLocalShadowPosition = localShadowMatrix * worldPosition;
    fragCurrentClipPosition = projection * view * worldPosition;
    fragPreviousClipPosition = previousViewProjection * activePreviousModel * vec4(inPosition, 1.0);
    gl_Position = fragCurrentClipPosition;
}
