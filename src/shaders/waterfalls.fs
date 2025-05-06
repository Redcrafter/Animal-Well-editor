#version 330 core

in vec2 TexCoords;
in vec4 Color;

out vec4 FragColor;

uniform float time;
uniform vec4 fgAmbientLightColor;
uniform vec4 ambientLightColor;

uniform sampler2D lightMask;
uniform sampler2D foregroundLight;
uniform sampler2D mainWindow;

uniform vec2 viewportOffset;
uniform vec2 viewportScale;

const vec2 viewportSize = vec2(320, 176);

void main() {
  vec2 v0 = gl_FragCoord.xy;
  vec2 v2 = TexCoords;

  vec4 r0,r1,r2,r3,r4,r5;
  r0.x = time * 23 + v0.x;
  r0.y = 2.7 * v0.x;
  r0.x = -v0.y * 0.3 + r0.x;
  r0.y = time * 13 + r0.y;
  r0.y = -v0.y * 0.2 + r0.y;
  r0.xy = sin(r0.xy);
  r0.x = r0.x + r0.y;
  r0.x = r0.x * 0.5 + 0.5;
  r0.y = 0.125 * r0.x;
  r0.x = r0.x * 0.125 + 0.875;
  if ((r0.x < v2.x) || (v2.x < r0.y)) discard;

  r0.xy = vec2(5,10) * time;
  r0.x = -v0.x * 0.5 + r0.x;
  r0.x = sin(r0.x);
  r0.y = sin(-r0.y);
  r0.x = r0.x + r0.y;
  r0.x = r0.x * 0.5 + 0.5;
  r0.y = 0.125 * r0.x;
  if (v2.y < r0.y) discard;

  r0.y = sin(v0.x);
  r1.xyzw = vec4(2.8,0.25,10,3) * v0.xyxx;
  r0.z = r1.x + r1.y;
  r0.w = 3 * time;
  r0.z = -time * 13.7 + r0.z;
  r0.z = sin(r0.z);
  r0.y = r0.y + r0.z;
  r0.z = dot(v0.xy, vec2(2.1,0.0625));
  r0.z = -time * 11 + r0.z;
  r0.z = sin(r0.z);
  r0.y = r0.y + r0.z;
  r0.z = v0.y * 0.25 + -r0.w;
  r0.z = sin(r0.z);
  r1.y = r0.z * 0.8 + r0.y;
  r1.x = v0.x + r1.y;
  r0.x = 0.1875 * r0.x;
  r0.x = ((v2.y < r0.x) || (-1.5 >= r1.y)) ? 1 : 0;
  r2.xz = vec2(4,0);
  r2.y = v0.y;
  r1.xy = r2.xy + r1.xy;
  r1.xy = r1.xy / viewportSize;
  r3.xyz = texture(lightMask, r1.xy * viewportScale + viewportOffset).xyz;
  r4.xyz = texture(foregroundLight, r1.xy * viewportScale + viewportOffset).xyz;

  r1.x = time * 7 + r1.z;
  r1.y = 24.3 * time;
  r1.z = time * 19.2 + r1.w;
  r1.xz = sin(r1.xz);
  r1.x = r1.x + r1.z;
  r1.y = sin(v0.x * 7 + r1.y);
  r1.x = r1.x + r1.y;
  r2.w = 0.5 * r1.x;
  r1.xy = v0.xy + r2.zw;
  r1.xy = r1.xy / viewportSize.xy;
  r1.xyz = texture(mainWindow, r1.xy * viewportScale + viewportOffset).xyz;
  r2.xyz = vec3(0.8,0.8,1) * r1.xyz;
  r5.xyz = vec3(0.2, 0.2, 1) - r1.xyz * vec3(0.8, 0.8, 1);
  r0.yzw = r5.xyz * vec3(0.0125000002,0.0125000002,0.0125000002) + r2.xyz;
  r2.xyz = r4.xyz + r3.xyz;
  r3.xyz = fgAmbientLightColor.xyz + ambientLightColor.xyz;
  r2.xyz = r3.xyz * r2.xyz;
  if((v2.x < 0.125) || (0.875 < v2.x)) {
    r3.xyz = ambientLightColor.xyz * vec3(0.4,0.4,1) - r1.xyz;
    r3.yzw = r3.xyz * 0.25 + r1.xyz;
    r3.x = 1;
    r0 = r3;
  }
  FragColor = vec4(Color.xyz * (r2.xyz * r0.x + r0.yzw), 1);
}
