#version 330 core

in vec2 TexCoords;
out vec4 FragColor;

uniform sampler2D tex;

void main() {
  vec3 col = texture(tex, TexCoords).rgb;
  float brightness  = dot(col, vec3(0.212599993, 0.7152, 0.0722));

  if (brightness > 0) {
    FragColor = vec4(0.5 * brightness * col, 1);
  } else {
    FragColor = vec4(0, 0, 0, 1);
  }
}
