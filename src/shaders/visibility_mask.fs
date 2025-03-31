#version 330 core

out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D visibility;

uniform vec2 viewportSize;
uniform float shadowBrightness;
uniform float time;

void main() {
  vec4 r0;
  vec2 r1;

  r0.xy = uvec2(viewportSize * TexCoords);

  float idk = viewportSize.y * TexCoords.y + time * 0.1 + viewportSize.x * uint(r0.x);
  r1.x = sin(idk) + sin(-3.90015626 * idk) + cos(idk * 0.114 + 3.7);

  r0.zw = ivec2(r0.xy) * ivec2(100,100);
  r0.xy = 1 - (ivec2(r0.xy) & ivec2(1,1));
  r0.x = int(r0.y) ^ int(r0.x);

  r1.y = fract(43758.5469 * sin(dot(uvec2(r0.zw), vec2(12.9898005,78.2330017)))) * 2 - 1;

  r0.yz = TexCoords + r1.xy / (vec2(1.25, 10.0 / 9.0) * viewportSize);

  float vis = texture(visibility, r0.yz).x;

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

  shit = min(shit, (r0.y - shit) * 0.4 + shit);

  vec4 r2 = vec4(0, 0, clamp(0.5 * shit, 0, 1), 1);
  FragColor = shit * (1 - r2) + r2;
}
