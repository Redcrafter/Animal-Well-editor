#version 330 core

out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D foreground;
uniform sampler2D midGround;
uniform sampler2D backgroundTiles;

void main() {
  FragColor.x = 1.0;
  FragColor.y = texture(backgroundTiles, TexCoords).w;
  FragColor.z = texture(foreground, TexCoords).w;
  FragColor.w = texture(midGround, TexCoords).w;
}
