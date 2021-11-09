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

vec4 over(const vec4 a, const vec4 b)
{
    const vec3 color = a.rgb + b.rgb * (1. - a.a);
    const float alpha = a.a + b.a * (1. - a.a);
    return vec4(color, alpha);
}

void main()
{
    const Material mat = materials.mat[matId];
    const vec4 tex = texture(textures[texId], uv);
    //const vec4 C = vec4(mat.r * tex.r, mat.g * tex.g, mat.b * tex.b, tex.a);
    const vec4 C = vec4(tex.r, tex.r, tex.r, tex.r);
    float b = 0.01;
    const vec4 uvColor = vec4(b, b, b, 1);
    const vec4 comp = over(C, uvColor);
    float L = dot(N, vec3(0, 0, 1));
    vec3 RGB  = L * vec3(comp.r, comp.g, comp.b);
    outColor = vec4(RGB, comp.a);
}
