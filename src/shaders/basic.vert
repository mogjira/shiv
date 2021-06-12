#version 460

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 uvw;

layout(location = 0) out vec3 outColor;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec2 outUv;

layout(set = 0, binding = 0) uniform Matrices {
    mat4 model;
    mat4 view;
    mat4 proj;
} matrices;

void main()
{
    gl_Position = matrices.proj * matrices.view * matrices.model * vec4(pos, 1.0);
    outColor = vec3(1, 1, 1);
    outNormal = mat3(matrices.view) * mat3(matrices.model) * normal;
    outUv = uvw;
    //gl_Position.z = (gl_Position.z + gl_Position.w) / 2.0; // for opengl compatibility
}
