#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inUv;

layout(set = 0, binding = 1) uniform CameraUBO {
    mat4 projection;
} camera;

layout(push_constant) uniform QuadPushConstants {
    uint drawMode;
    float timeSeconds;
} quadPush;

layout(location = 0) out vec2 fragUv;

void main() {
    gl_Position = vec4((camera.projection * vec4(inPosition, 1.0)).xy, 0.0, 1.0);
    fragUv = inUv;
}
