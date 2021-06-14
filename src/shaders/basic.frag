#version 460
#extension GL_GOOGLE_include_directive : enable

vec4 over(const vec4 a, const vec4 b)
{
    //const vec3 color = a.rgb * a.a + b.rgb * b.a * (1. - a.a);
    const vec3 color = a.rgb + b.rgb * (1. - a.a);
    const float alpha = a.a + b.a * (1. - a.a);
    return vec4(color, alpha);
}

layout(location = 0) in  vec3 N;
layout(location = 1) in  vec2 UV;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform Matrices {
    mat4 view;
    mat4 proj;
} matrices;

void main()
{
    float L = dot(N, vec3(0, 0, 1));
    vec3 C  = L * vec3(1, 1, 1);
    outColor = vec4(C, 1.0);
}
