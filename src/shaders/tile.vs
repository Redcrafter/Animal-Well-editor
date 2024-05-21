#version 330 core

layout(location = 0) in vec4 vertexPosition;

out vec2 UV;

uniform mat4 MVP;

void main() {
	gl_Position = MVP * vec4(vertexPosition.xy, 0, 1);
	UV = vertexPosition.zw;
}
