#version 450

layout(push_constant) uniform QuadPushConstants {
    uint drawMode;
    float timeSeconds;
    uint flipUvU;
    uint flipUvV;
    mat4 mvp;
} quadPush;

layout(set = 0, binding = 0) uniform sampler2D texSampler;

layout(location = 0) in vec2 fragUv;

layout(location = 0) out vec4 outColor;

const uint kDrawTextured = 0u;
const uint kDrawWireframeOverlay = 1u;

void main() {
    if (quadPush.drawMode == kDrawWireframeOverlay) {
        outColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    vec2 uv = fragUv;
    if (quadPush.flipUvU != 0u) {
        uv.x = 1.0 - uv.x;
    }
    if (quadPush.flipUvV != 0u) {
        uv.y = 1.0 - uv.y;
    }
    vec4 c = texture(texSampler, uv);
    const float kAlphaCutoff = 0.5;
    if (c.a < kAlphaCutoff) {
        discard;
    }
    outColor = c;
}
