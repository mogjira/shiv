#version 460
#extension GL_GOOGLE_include_directive : enable

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 norm;
layout(location = 2) in vec2 uvw;

layout(location = 0) out vec3 outWorldPos;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec2 outUv;
layout(location = 3) out uint outMatId;
layout(location = 4) out uint outTexId;

layout(set = 0, binding = 0) uniform Camera {
    mat4 view;
    mat4 proj;
} camera;

layout(push_constant) uniform PushConstant {
    mat4 xform;
    uint primId;
    uint matId;
    uint texId;
} push;

void main()
{
    vec4 worldPos = push.xform * vec4(pos, 1.0);
    gl_Position = camera.proj * camera.view * worldPos;
    outWorldPos = worldPos.xyz; 
    outNormal = normalize((camera.view * push.xform * vec4(norm, 0.0)).xyz); // this is fine as long as we only allow uniform scales
    outUv = uvw.st;
    outMatId = push.matId;
}
