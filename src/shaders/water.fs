#version 330 core

in vec2 TexCoords;
out vec4 FragColor;

uniform sampler2D tex;
uniform vec2 viewportSize;
uniform float time;

void main() {
  vec2 coords = gl_FragCoord.xy / viewportSize;

  float dy = -(viewportSize.y * TexCoords.y);
  vec2 r1;
  r1.y = coords.y - TexCoords.y * 2;
  r1.x = coords.x + (dy / 2500.0) * sin(TexCoords.x * 63 + r1.y * (viewportSize.y / 176) * 200 + time) / (viewportSize.x / 320);
  vec4 color = texture(tex, r1);

  float brightness = 1 - clamp(-dy / 60, 0, 1);
  FragColor = vec4(color.xyz * brightness * vec3(0.5, 0.5, 0.7), color.w);
}
