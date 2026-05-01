#version 450

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inUv;

layout(set = 0, binding = 1) uniform CameraUBO {
    mat4 projection;
} camera;

layout(location = 0) out vec2 fragUv;

void main() {
    gl_Position = camera.projection * vec4(inPosition, 0.0, 1.0);
    fragUv = inUv;
}
