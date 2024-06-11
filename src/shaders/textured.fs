#version 330 core

in vec2 UV;
out vec4 result;

uniform sampler2D atlas;
uniform vec4 color;

void main() {
    result = texture(atlas, UV) * color;
}
