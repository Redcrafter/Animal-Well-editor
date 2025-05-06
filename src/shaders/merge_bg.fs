#version 330 core

out vec4 FragColor;
in vec2 TexCoords;

uniform vec4 lights[16];

uniform vec4 ambientLightColor;
uniform vec4 fgAmbientLightColor;
uniform vec4 bgAmbientLightColor;
uniform vec4 fogColor;

uniform float midToneBrightness;
uniform float shadowBrightness;
uniform float time;

uniform sampler2D tex; // background tiles
uniform sampler2D lightMask;
uniform sampler2D visibility;
uniform sampler2D foregroundLight;
uniform sampler2D backgroundNormals;
uniform sampler2D seperatedLights;

uniform vec2 viewportOffset;
uniform vec2 viewportScale;

const vec2 viewportSize = vec2(320, 176);

void main() {
  vec2 v0 = TexCoords * viewportSize;

  vec4 r0,r1,r2,r3,r4,r5,r6,r7,r8,r9;

  r0.xy = v0 / viewportSize;
  r0.zw = texture(visibility, TexCoords * viewportScale + viewportOffset).zw;
  if (r0.z == 1 || r0.w == 1) discard;

  r1 = texture(tex, TexCoords * viewportScale + viewportOffset);
  if (r1.w == 0 || r1.xyz == vec3(0, 1, 1)) discard;

  r2.y = 0.5 * time;
  r2.z = 0;
  r0.zw = v0 + r2.yz;
  r2.x = dot(r0.zw, vec2(0.366025418,0.366025418));
  r2.xy = r2.xx + r0.zw;
  r2.xy = floor(r2.xy);
  r0.zw = -r2.xy + r0.zw;
  r2.z = dot(r2.xy, vec2(0.211324871,0.211324871));
  r0.zw = r2.zz + r0.zw;
  r3.xyzw = vec4(0.211324871,0.211324871,-0.577350259,-0.577350259) + r0.zwzw;
  r4.xyzw = (r0.w < r0.z) ? vec4(0,1,-1,-0) : vec4(1,0,-0,-1);
  r3.xy = r4.zw + r3.xy;
  r2.zw = vec2(0.00346020772,0.00346020772) * r2.xy;
  r2.zw = floor(r2.zw);
  r2.xy = -r2.zw * vec2(289,289) + r2.xy;
  r5.xz = vec2(0,1);
  r5.y = r4.x;
  r2.yzw = r5.xyz + r2.yyy;
  r5.xyz = r2.yzw * vec3(34,34,34) + vec3(1,1,1);
  r2.yzw = r5.xyz * r2.yzw;
  r5.xyz = vec3(0.00346020772,0.00346020772,0.00346020772) * r2.yzw;
  r5.xyz = floor(r5.xyz);
  r2.yzw = -r5.xyz * vec3(289,289,289) + r2.yzw;
  r2.xyz = r2.yzw + r2.xxx;
  r4.xz = vec2(0,1);
  r2.xyz = r4.xyz + r2.xyz;
  r4.xyz = r2.xyz * vec3(34,34,34) + vec3(1,1,1);
  r2.xyz = r4.xyz * r2.xyz;
  r4.xyz = vec3(0.00346020772,0.00346020772,0.00346020772) * r2.xyz;
  r4.xyz = floor(r4.xyz);
  r2.xyz = -r4.xyz * vec3(289,289,289) + r2.xyz;
  r4.x = dot(r0.zw, r0.zw);
  r4.y = dot(r3.xy, r3.xy);
  r4.z = dot(r3.zw, r3.zw);
  r4.xyz = vec3(0.5,0.5,0.5) + -r4.xyz;
  r4.xyz = max(vec3(0,0,0), r4.xyz);
  r4.xyz = r4.xyz * r4.xyz;
  r4.xyz = r4.xyz * r4.xyz;
  r2.xyz = vec3(0.024390243,0.024390243,0.024390243) * r2.xyz;
  r2.xyz = fract(r2.xyz);
  r5.xyz = r2.xyz * vec3(2,2,2) + vec3(-1,-1,-1);
  r6.xyz = vec3(-0.5,-0.5,-0.5) + abs(r5.xyz);
  r2.xyz = r2.xyz * vec3(2,2,2) + vec3(-0.5,-0.5,-0.5);
  r2.xyz = floor(r2.xyz);
  r2.xyz = r5.xyz + -r2.xyz;
  r5.xyz = r6.xyz * r6.xyz;
  r5.xyz = r2.xyz * r2.xyz + r5.xyz;
  r5.xyz = -r5.xyz * vec3(0.853734732,0.853734732,0.853734732) + vec3(1.79284286,1.79284286,1.79284286);
  r4.xyz = r5.xyz * r4.xyz;
  r0.w = r6.x * r0.w;
  r5.x = r2.x * r0.z + r0.w;
  r0.zw = r6.yz * r3.yw;
  r5.yz = r2.yz * r3.xz + r0.zw;
  r2.x = dot(r4.xyz, r5.xyz);
  r0.zw = vec2(100,100) * v0.xy;
  r3.x = time;
  r3.yz = vec2(1000,1000);
  r2.zw = v0.xy * vec2(0.5,0.5) + r3.xy;
  r3.w = 0.200000003 * time;
  r2.zw = r3.zw + r2.zw;
  r3.x = dot(r2.zw, vec2(0.366025418,0.366025418));
  r3.xy = r3.xx + r2.zw;
  r3.xy = floor(r3.xy);
  r2.zw = -r3.xy + r2.zw;
  r3.z = dot(r3.xy, vec2(0.211324871,0.211324871));
  r2.zw = r3.zz + r2.zw;
  r4.xyzw = vec4(0.211324871,0.211324871,-0.577350259,-0.577350259) + r2.zwzw;
  r5.xyzw = (r2.w < r2.z) ? vec4(0,1,-1,-0) : vec4(1,0,-0,-1);
  r4.xy = r5.zw + r4.xy;
  r3.zw = vec2(0.00346020772,0.00346020772) * r3.xy;
  r3.zw = floor(r3.zw);
  r3.xy = -r3.zw * vec2(289,289) + r3.xy;
  r6.xz = vec2(0,1);
  r6.y = r5.x;
  r3.yzw = r6.xyz + r3.yyy;
  r6.xyz = r3.yzw * vec3(34,34,34) + vec3(1,1,1);
  r3.yzw = r6.xyz * r3.yzw;
  r6.xyz = vec3(0.00346020772,0.00346020772,0.00346020772) * r3.yzw;
  r6.xyz = floor(r6.xyz);
  r3.yzw = -r6.xyz * vec3(289,289,289) + r3.yzw;
  r3.xyz = r3.yzw + r3.xxx;
  r5.xz = vec2(0,1);
  r3.xyz = r5.xyz + r3.xyz;
  r5.xyz = r3.xyz * vec3(34,34,34) + vec3(1,1,1);
  r3.xyz = r5.xyz * r3.xyz;
  r5.xyz = vec3(0.00346020772,0.00346020772,0.00346020772) * r3.xyz;
  r5.xyz = floor(r5.xyz);
  r3.xyz = -r5.xyz * vec3(289,289,289) + r3.xyz;
  r5.x = dot(r2.zw, r2.zw);
  r5.y = dot(r4.xy, r4.xy);
  r5.z = dot(r4.zw, r4.zw);
  r5.xyz = vec3(0.5,0.5,0.5) + -r5.xyz;
  r5.xyz = max(vec3(0,0,0), r5.xyz);
  r5.xyz = r5.xyz * r5.xyz;
  r5.xyz = r5.xyz * r5.xyz;
  r3.xyz = vec3(0.024390243,0.024390243,0.024390243) * r3.xyz;
  r3.xyz = fract(r3.xyz);
  r6.xyz = r3.xyz * vec3(2,2,2) + vec3(-1,-1,-1);
  r7.xyz = vec3(-0.5,-0.5,-0.5) + abs(r6.xyz);
  r3.xyz = r3.xyz * vec3(2,2,2) + vec3(-0.5,-0.5,-0.5);
  r3.xyz = floor(r3.xyz);
  r3.xyz = r6.xyz + -r3.xyz;
  r6.xyz = r7.xyz * r7.xyz;
  r6.xyz = r3.xyz * r3.xyz + r6.xyz;
  r6.xyz = -r6.xyz * vec3(0.853734732,0.853734732,0.853734732) + vec3(1.79284286,1.79284286,1.79284286);
  r5.xyz = r6.xyz * r5.xyz;
  r2.w = r7.x * r2.w;
  r6.x = r3.x * r2.z + r2.w;
  r2.zw = r7.yz * r4.yw;
  r6.yz = r3.yz * r4.xz + r2.zw;
  r2.y = dot(r5.xyz, r6.xyz);
  r2.z = viewportSize.x * v0.x;
  r2.z = time * 0.1 + r2.z;
  r2.z = v0.y + r2.z;
  r2.w = sin(r2.z);
  r3.x = 3.90015626 * r2.z;
  r3.x = sin(-r3.x);
  r2.w = r3.x + r2.w;
  r2.z = r2.z * 0.114 + 3.7;
  r2.z = cos(r2.z);
  r3.x = r2.w + r2.z;
  r0.z = dot(r0.zw, vec2(12.9898005,78.2330017));
  r0.z = sin(r0.z);
  r0.z = 43758.5469 * r0.z;
  r0.z = fract(r0.z);
  r3.y = r0.z * 2 + -1;
  r0.zw = vec2(-1.5,-0.5) + v0;
  r0.zw = r0.zw / viewportSize.xy;
  r3.xyzw = r3.xyxy * vec4(0.0025,0.005,0.00075,0.0015) + r0.zwzw;
  r0.z = texture(visibility, r3.xy * viewportScale + viewportOffset).x;
  r2.zw = texture(visibility, r3.zw * viewportScale + viewportOffset).zw;
  r3.xyz = texture(backgroundNormals, r0.xy * viewportScale + viewportOffset).xyz;
  r3.xyz = r3.xyz * vec3(2,2,2) + vec3(-1,-1,-1);
  r0.w = inversesqrt(dot(r3.xyz, r3.xyz));
  r3.xyw = r3.xyz * r0.www;
  r4.xy = r2.xy * vec2(0.65,0.65) + r0.xy;
  r5.xyz = texture(lightMask, r4.xy * viewportScale + viewportOffset).xyz;
  r0.w = -r3.z * r0.w + 1;
  r0.w = r0.w * 0.5 + r3.w;
  r5.xyz = r5.xyz * r0.www;
  r0.xy = vec2(0,-0.05) + r0.xy;
  r6.xyzw = texture(foregroundLight, r0.xy * viewportScale + viewportOffset).xyzw;
  r0.x = v0.x * viewportSize.x + time;
  r0.x = v0.y + r0.x;
  r0.y = sin(r0.x);
  r3.z = 3.90015626 * r0.x;
  r3.z = sin(-r3.z);
  r0.y = r3.z + r0.y;
  r0.x = r0.x * 0.114 + 3.70000005;
  r0.x = cos(r0.x);
  r0.x = r0.y + r0.x;
  r7.z = 32;
  r8.xyz = r5.xyz;

  for(int i = 0; i < 16; i++) {
    vec4 light = lights[i];

    if (light.z > 0) {
      r7.xy = light.xy - v0;
      float dist = length(r7.xy);

      if (dist < light.z) {
        // r9 = texture(seperatedLights, (0.25 * ((r7.xy / viewportSize) * 0.05 + r4.xy + vec2(i & 3, i >> 2))) * viewportScale + viewportOffset).xyz;
        vec3 r9 = texture(seperatedLights, ((r7.xy / viewportSize) * 0.05 + r4.xy) * viewportScale + viewportOffset).xyz;

        float vis = texture(visibility, ((r2.xy * 130 + (light.xy - r7.xy * 0.8)) / viewportSize) * viewportScale + viewportOffset).w;

        r8.xyz += r9 * (floor(clamp(dot(r9, r3.xyw), 0, 1) * 2 + 0.5 + r0.x * 0.02) * 0.25 + 0.5) * 0.5 * floor(2.5 - vis * 2);
      }
    }
  }

  r8.xyz = clamp(r8.xyz, 0, 1);
  r0.y = max(r2.z, r2.w);
  r0.y = 1 + -r0.y;
  r0.y = (r0.y >= 0.85) ? 1 : 0;
  r2.x = dot(vec2(0.6,0.8), r3.yw);
  r0.x = r0.x * 0.01 + r2.x;
  r0.x = 0.25 + r0.x;
  r0.x = r0.x + r0.x;
  r0.x = floor(r0.x);
  r0.x = 0.5 * r0.x;
  r2.x = midToneBrightness * shadowBrightness;
  r2.y = -midToneBrightness * shadowBrightness + midToneBrightness;
  r2.x = r0.y * r2.y + r2.x;
  r2.xyzw = ambientLightColor.xyzw * r2.xxxx;
  r3.xyzw = clamp(fgAmbientLightColor.xyzw * r0.xxxx + bgAmbientLightColor.xyzw, 0, 1);
  r4.xyzw = r3.xyzw * r2.xyzw;
  r6.xyz = r6.xyz * r0.yyy;
  r5.xyzw = fogColor.xyzw + -r1.xyzw;
  r1.xyzw = fogColor.wwww * r5.xyzw + r1.xyzw;
  r4.xyz = shadowBrightness * r4.xyz;
  r0.x = (r0.z >= 0.97) ? 1 : 0;
  r2.xyz = r2.xyz * r3.xyz + -r4.xyz;
  r2.xyz = r0.xxx * r2.xyz + r4.xyz;
  r8.w = r4.w;
  r0.xyzw = r6.xyzw * r0.wwww + r8.xyzw;
  r2.w = 1;
  r0.xyzw = r2.xyzw + r0.xyzw;
  FragColor = r1.xyzw * r0.xyzw;
}
