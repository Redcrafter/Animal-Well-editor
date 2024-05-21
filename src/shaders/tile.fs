#version 330 core

in vec2 UV;

out vec4 result;

uniform sampler2D atlas;
uniform vec4 color;

void main() {
    vec4 col = texture(atlas, UV);

    if(col.rgb == vec3(0, 1, 1))
        discard;

    result = col * color;
}
