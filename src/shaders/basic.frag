#version 460
#extension GL_GOOGLE_include_directive : enable

vec4 over(const vec4 a, const vec4 b)
{
    //const vec3 color = a.rgb * a.a + b.rgb * b.a * (1. - a.a);
    const vec3 color = a.rgb + b.rgb * (1. - a.a);
    const float alpha = a.a + b.a * (1. - a.a);
    return vec4(color, alpha);
}

layout(location = 0) in  vec3 inColor;
layout(location = 1) in  vec3 inNormal;
layout(location = 2) in  vec2 inUv;

layout(location = 0) out vec4 outColor;

void main()
{
    outColor = vec4(1.0, 1.0, 1.0, 1.0);
}
