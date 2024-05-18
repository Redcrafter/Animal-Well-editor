#version 330 core

in vec2 UV;
// in vec4 Color;

out vec3 result;

uniform sampler2D myTextureSampler;

void main() {
    vec4 col = texture(myTextureSampler, UV); //  * Color

    if(col.rgb == vec3(0, 1, 1))
        discard;

    result = col.rgb;
}
