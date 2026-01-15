#version 330 core

in vec3 LineColor;
out vec4 FragColor;

void main() {
    FragColor = vec4(LineColor, 1.0);
}