#version 450

layout(location = 0) in vec3 vNormal;
layout(location = 1) in vec2 vUV;

layout(set = 0, binding = 0) uniform sampler2D texSampler;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 n = normalize(vNormal);
    vec3 lightDir = normalize(vec3(0.35, 0.85, 0.55));
    float diffuse = max(dot(n, lightDir), 0.0);
    float rim = pow(1.0 - max(n.z, 0.0), 3.0);

    vec4 texel = texture(texSampler, vUV);
    vec3 base = texel.rgb;
    vec3 color = base * (0.35 + 0.65 * diffuse);
    color += vec3(0.05, 0.45, 0.95) * rim * 0.25;
    outColor = vec4(color, texel.a);
}
