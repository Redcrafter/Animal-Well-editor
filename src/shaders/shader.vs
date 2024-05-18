#version 330 core

layout(location = 0) in vec4 vertexPosition;
// layout(location = 1) in vec3 vertexNormal;
// layout(location = 2) in vec3 vertexColor;
// layout(location = 3) in vec2 vertexUV;

out vec2 UV;
// out vec4 Color;

// Values that stay constant for the whole mesh.
uniform mat4 MVP;

void main() {
    // Output position of the vertex, in clip space : MVP * position
	gl_Position = MVP * vec4(vertexPosition.xy, 1, 1);

	UV = vertexPosition.zw;
	// Color = vec4(vertexColor, 1);
}
