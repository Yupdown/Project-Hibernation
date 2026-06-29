#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inUv;

layout(push_constant) uniform QuadPushConstants {
    uint drawMode;
    float timeSeconds;
    uint flipUvU;
    uint flipUvV;
    mat4 mvp;
} quadPush;

layout(location = 0) out vec2 fragUv;

void main() {
    gl_Position = quadPush.mvp * vec4(inPosition, 1.0);
    fragUv = inUv;
}
