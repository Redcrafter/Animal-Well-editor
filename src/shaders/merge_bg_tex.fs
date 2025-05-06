#version 330 core

in vec2 TexCoords;
out vec4 FragColor;

uniform vec4 lights[16];

uniform vec4 ambientLightColor;
uniform vec4 fogColor;
uniform float time;

uniform vec2 viewportOffset;
uniform vec2 viewportScale;

uniform sampler2D tex; // background
uniform sampler2D foregroundLight;
uniform sampler2D visibility;
uniform sampler2D seperatedLights;

const vec2 viewportSize = vec2(320, 176);
const float farBackgroundReflectivity = 1;

void main() {
  vec4 r0,r1,r2,r3,r6,r7;


  vec2 v0 = mod(gl_FragCoord.xy, viewportSize);
  vec2 v2 = v0 / viewportSize;

  r0.xyzw = texture(tex, v2.xy * viewportScale + viewportOffset).xyzw;
  r1.xyz = fogColor.xyz + -r0.xyz;
  r0.xyz = fogColor.www * r1.xyz + r0.xyz;
  r1.xyz = texture(foregroundLight, v2.xy * viewportScale + viewportOffset).xyz;
  r1.xyz = farBackgroundReflectivity * r1.xyz;
  r1.xyz = r1.xyz * vec3(0.25,0.25,0.25) + ambientLightColor.xyz;
  r1.xyz = r1.xyz * r0.xyz;
  r2.x = 0.5 * time;
  r2.yw = vec2(0,1000);
  r2.xy = v0.xy + r2.xy;
  r1.w = dot(r2.xy, vec2(0.366025418,0.366025418));
  r3.xy = r2.xy + r1.ww;
  r3.xy = floor(r3.xy);
  r2.xy = -r3.xy + r2.xy;
  r1.w = dot(r3.xy, vec2(0.211324871,0.211324871));
  r2.xy = r2.xy + r1.ww;
  vec4 r4 = vec4(0.211324871,0.211324871,-0.577350259,-0.577350259) + r2.xyxy;
  vec4 r5 = (r2.y < r2.x) ? vec4(0,1,-1,-0) : vec4(1,0,-0,-1);
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
  r5.x = dot(r2.xy, r2.xy);
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
  r1.w = r7.x * r2.y;
  r6.x = r3.x * r2.x + r1.w;
  r2.xy = r7.yz * r4.yw;
  r6.yz = r3.yz * r4.xz + r2.xy;
  r2.x = dot(r5.xyz, r6.xyz);
  r2.z = time;
  r2.zw = v0.xy * vec2(0.5,0.5) + r2.zw;
  r3.x = 1000;
  r3.y = 0.200000003 * time;
  r2.zw = r3.xy + r2.zw;
  r1.w = dot(r2.zw, vec2(0.366025418,0.366025418));
  r3.xy = r2.zw + r1.ww;
  r3.xy = floor(r3.xy);
  r2.zw = -r3.xy + r2.zw;
  r1.w = dot(r3.xy, vec2(0.211324871,0.211324871));
  r2.zw = r2.zw + r1.ww;
  r4 = vec4(0.211324871,0.211324871,-0.577350259,-0.577350259) + r2.zwzw;
  r5 = (r2.w < r2.z) ? vec4(0,1,-1,-0) : vec4(1,0,-0,-1);
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
  r6.xyz = 1.79284286 - r6.xyz * 0.853734732;
  r5.xyz = r6.xyz * r5.xyz;
  r1.w = r7.x * r2.w;
  r6.x = r3.x * r2.z + r1.w;
  r2.zw = r7.yz * r4.yw;
  r6.yz = r3.yz * r4.xz + r2.zw;
  r2.y = dot(r5.xyz, r6.xyz);
  r0.xyz = farBackgroundReflectivity * r0.xyz;
  r3.xyz = r1.xyz;
  r2.zw = r2.xy * 0.65 + v2.xy;

  for(int i = 0; i < 16; i++) {
    vec4 light = lights[i];

    if (0 < light.z) {
      r4.xy = light.xy - v0.xy;

      if (length(r4.xy) < light.z) {
        // #todo use real seperated lights
        // r5.xyz = texture(seperatedLights, ((r4.xy / viewportSize.xy) * 0.05 + r2.zw + vec2(i & 3, i >> 2)) * 0.25).xyz;
        r5.xyz = texture(seperatedLights, ((r4.xy / viewportSize) * 0.05 + r2.zw) * viewportScale + viewportOffset).xyz;
        r4.xy = 1 - texture(visibility, ((r2.xy * 130 + (light.xy - r4.xy * 0.65)) / viewportSize) * viewportScale + viewportOffset).yw;

        if(r4.x * r4.y >= 0.7) {
          r3.xyz += r5.xyz * r0.xyz;
        }
      }
    }
  }

  FragColor = vec4(r3.xyz, r0.w);
}
