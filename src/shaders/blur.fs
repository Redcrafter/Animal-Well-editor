#version 330 core

in vec2 TexCoords;
out vec4 FragColor;

uniform bool horizontal;
uniform sampler2D tex;

const float weights[4] = float[](0.383103013, 0.241843, 0.0606260002, 0.0059799999);

void main() {
  vec2 texelSize = 1.0 / textureSize(tex, 0);

  // vec2 r2 = vec2(1 - texelSize.x * 0.5, 0.5 * texelSize.x);

  // vec4 result = texture(tex, min(max(TexCoords, r2.yy), r2.xx)) * weights[0];
  vec4 result = texture(tex, TexCoords) * weights[0];
  if(horizontal) {
    for(int i = 1; i <= 3; i++) {
      // result += texture(tex, min(max(TexCoords + vec2(i * texelSize.x, 0), r2.yy), r2.xx)) * weights[i];
      // result += texture(tex, min(max(TexCoords - vec2(i * texelSize.x, 0), r2.yy), r2.xx)) * weights[i];

      result += texture(tex, TexCoords + vec2(i * texelSize.x, 0)) * weights[i];
      result += texture(tex, TexCoords - vec2(i * texelSize.x, 0)) * weights[i];
    }
  } else {
    for(int i = 1; i <= 3; i++) {
      // result += texture(tex, min(max(TexCoords + vec2(0, i * texelSize.y), r2.yy), r2.xx)) * weights[i];
      // result += texture(tex, min(max(TexCoords - vec2(0, i * texelSize.y), r2.yy), r2.xx)) * weights[i];

      result += texture(tex, TexCoords + vec2(0, i * texelSize.y)) * weights[i];
      result += texture(tex, TexCoords - vec2(0, i * texelSize.y)) * weights[i];
    }
  }

  FragColor = clamp(result, 0, 1);
}
