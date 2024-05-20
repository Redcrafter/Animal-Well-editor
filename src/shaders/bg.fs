#version 330 core

in vec3 UV;
out vec4 result;

uniform sampler2DArray myTextureSampler;
uniform vec4 color;

void main() {
    result = texture(myTextureSampler, UV) * color;
}
