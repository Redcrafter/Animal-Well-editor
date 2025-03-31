#version 330 core

layout(location = 0) in vec2 vertexPosition;
layout(location = 1) in vec2 vertexUv;
layout(location = 2) in vec4 vertexColor;

out vec2 TexCoords;
out vec4 Color;

void main() {
    gl_Position = vec4(vertexPosition, 0.0, 1.0);
    TexCoords = vertexUv;
    Color = vertexColor;
}
