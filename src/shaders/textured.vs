#version 330 core

layout(location = 0) in vec2 vertexPosition;
layout(location = 1) in vec2 vertexUv;
layout(location = 2) in vec4 vertexColor;

out vec2 UV;
out vec4 Color;

uniform mat4 MVP;

void main() {
	gl_Position = MVP * vec4(vertexPosition, 0, 1);
	UV = vertexUv;
	Color = vertexColor;
}
