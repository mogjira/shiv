#version 460
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_nonuniform_qualifier : enable

struct Material {
    float r;
    float g;
    float b;
    float roughness;
};

layout(location = 0) in       vec3 worldPos;
layout(location = 1) in       vec3 N;
layout(location = 2) in       vec2 uv;
layout(location = 3) flat in  uint matId;
layout(location = 4) flat in  uint texId;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 1) uniform Materials {
    Material mat[16];
} materials;

layout(set = 0, binding = 2) uniform sampler2D textures[];

void main()
{
    const Material mat = materials.mat[matId];
    const vec3 tex = texture(textures[texId], uv).rgb;
    float L = dot(N, vec3(0, 0, 1));
    vec3 C  = L * vec3(mat.r * tex.r, mat.g * tex.r, mat.b * tex.r);
    outColor = vec4(C, 1.0);
}

