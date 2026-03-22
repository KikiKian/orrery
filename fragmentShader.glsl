#version 460 core
out vec4 fragColor;

void main() {
    vec2 coord = gl_PointCoord - vec2(0.5);
    if (length(coord) > 0.5)
        discard;
    fragColor = vec4(1.0, 1.0, 1.0, 1.0);
}
