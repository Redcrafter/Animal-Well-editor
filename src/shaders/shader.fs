#version 330 core

in vec2 UV;
// in vec4 Color;

out vec4 result;

uniform sampler2D myTextureSampler;
uniform vec4 color;

void main() {
    vec4 col = texture(myTextureSampler, UV);

    if(col.rgb == vec3(0, 1, 1))
        discard;

    result = col * color;
}
