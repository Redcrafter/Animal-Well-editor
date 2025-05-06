#version 330 core

in vec2 TexCoords;
out vec4 FragColor;

uniform sampler2D mainWindow;
uniform vec4 colorGainSaturation;

void main() {
  vec4 r0,r1,r2,r3;

  r0.zw = vec2(-1, 0.666666687);
  r1.zw = vec2(1, -1);
  r2 = texture(mainWindow, TexCoords);
  r2.xyz = log2(abs(r2.xyz));
  FragColor.w = r2.w;
  r2.xyz = colorGainSaturation.xyz * r2.xyz;
  r2.xyw = exp2(r2.yzx);
  r3.x = (r2.x >= r2.y) ? 1 : 0;
  r0.xy = r2.yx;
  r1.xy = r2.xy + -r0.xy;
  r0 = r3.x * r1 + r0;
  r2.xyz = r0.xyw;
  r1.x = (r2.w >= r2.x) ? 1 : 0;
  r0.xyw = r2.wyx;
  r0 = -r2 + r0;
  r0 = r1.x * r0 + r2;
  r1.x = min(r0.w, r0.y);
  r1.x = -r1.x + r0.x;
  r1.y = r1.x * 6 + 1e-10;
  r0.y = r0.w + -r0.y;
  r0.y = r0.y / r1.y;
  r0.y = r0.z + r0.y;
  r0.yzw = abs(r0.yyy) * 6 + vec3(0,4,2);
  r0.yzw = 0.166666672 * r0.yzw;
  r0.yzw = abs(fract(r0.yzw));
  r0.yzw = r0.yzw * 6 - 3;
  r0.yzw = clamp(abs(r0.yzw) - 1, 0, 1);
  r0.yzw = r0.yzw - 1;
  r1.y = 1e-10 + r0.x;
  r1.x = r1.x / r1.y;
  r1.x = colorGainSaturation.w * r1.x;
  r0.yzw = r1.xxx * r0.yzw + 1;
  FragColor.xyz = r0.xxx * r0.yzw;
}
