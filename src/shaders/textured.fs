#version 330 core

in vec2 TexCoords;
in vec4 Color;

out vec4 result;

uniform sampler2D atlas;
uniform vec4 color;

void main() {
    result = texture(atlas, TexCoords) * color * Color;
}
