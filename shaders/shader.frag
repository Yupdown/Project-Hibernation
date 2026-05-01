#version 450

layout(set = 0, binding = 0) uniform sampler2D texSampler;

layout(location = 0) in vec2 fragUv;

layout(location = 0) out vec4 outColor;

void main() {
    vec4 c = texture(texSampler, fragUv);
    const float kAlphaCutoff = 0.5;
    if (c.a < kAlphaCutoff) {
        discard;
    }
    outColor = c;
}
