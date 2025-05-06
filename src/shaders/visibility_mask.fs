#version 330 core

out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D visibility;

uniform vec2 viewportScale;
uniform float shadowBrightness;
uniform float time;

const vec2 viewportSize = vec2(320, 176);

void main() {
  vec4 r0;
  vec2 r1;

  vec2 v2 = mod(TexCoords / viewportScale, 1);
  vec2 v0 = v2 * viewportSize;

  r0.xy = uvec2(v0);
  r0.z = time * 0.1 + viewportSize.x * v0.x + v0.y;
  r0.w = sin(-3.90015626 * r0.z);
  r1.x = sin(r0.z);
  r0.z = cos(r0.z * 0.114 + 3.7);
  r0.w = r1.x + r0.w;
  r0.z = r0.w + r0.z;
  r1.x = 0.0025 * r0.z;

  r0.zw = ivec2(r0.xy) * ivec2(100,100);
  r0.xy = 1 - (ivec2(r0.xy) & ivec2(1,1));
  r0.x = int(r0.y) ^ int(r0.x);
  r0.y = fract(43758.5469 * sin(dot(uvec2(r0.zw), vec2(12.9898005,78.2330017)))) * 2 - 1;
  r1.y = 0.005 * r0.y;

  float vis = texture(visibility, TexCoords + r1.xy * viewportScale).x;

  float shit = 1;
  if(0.21 >= vis) {
    shit = 0;
  } else if(0.25 >= vis) {
    shit = shadowBrightness * r0.x;
  } else if(0.3 >= vis) {
    shit = shadowBrightness;
  } else if(0.395 >= vis) {
    shit = r0.x * (1 - shadowBrightness) + shadowBrightness;
  }

  shit = min(shit, (vis - shit) * 0.4 + shit);

  vec4 r2 = vec4(0, 0, clamp(0.5 * shit, 0, 1), 1);
  FragColor = shit * (1 - r2) + r2;
}
