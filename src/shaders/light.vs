#version 330 core

layout(location = 0) in vec2 vertexPosition;
layout(location = 2) in vec4 vertexColor;

out vec4 color;

uniform mat4 MVP;

void main() {
    // Output position of the vertex, in clip space : MVP * position
	gl_Position = MVP * vec4(vertexPosition, 0, 1);
	color = vertexColor;
}
