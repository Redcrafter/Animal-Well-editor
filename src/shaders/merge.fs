#version 330 core

out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D tiles;
uniform sampler2D lights;

void main() {
    vec4 tile = texture(tiles, TexCoords);
    vec4 light = texture(lights, TexCoords);

    if(light.rgb == vec3(0, 0, 0)) {
        light.rgb = vec3(0.2, 0.2, 0.2);
    }

    FragColor = tile * light;
}
