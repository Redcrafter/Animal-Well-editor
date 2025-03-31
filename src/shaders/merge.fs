#version 330 core

out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D foreground;
uniform sampler2D lightMask;
uniform sampler2D foregroundLight;
uniform sampler2D visibility;

uniform float rimLightBrightness;
uniform vec4 ambientLightColor;
uniform vec4 fgAmbientLightColor;

void main() {
    vec4 r0, r1;
    vec2 r2;

    r0.xyz = texture(lightMask, TexCoords).xyz;
    // r1.xyz = texture(foregroundLight, TexCoords).xyz + 0.2;
    r1.xyz = vec3(0.2);
    r2.xy = texture(visibility, TexCoords).xz;
    
    bool b1 = r2.y >= 0.7874;
    bool b2 = 0.35 >= r2.x;

    float shit = b1 ? 0.05 : 0.0;
    r0.w = b1 ? 0.0 : 1.0;

    shit = r0.w + shit;

    r0.xyz = shit * r0.xyz + r1.xyz;

    r1.xyz = b2 ? vec3(0, 0, 0) : (rimLightBrightness * ambientLightColor.xyz) * r0.www;
    r0.xyz = r0.xyz + r1.xyz;

    r0.xyz = ambientLightColor.xyz * fgAmbientLightColor.xyz + r0.xyz;

    vec4 fg = texture(foreground, TexCoords);
    FragColor.xyz = fg.xyz * r0.xyz;
    FragColor.w = fg.w;
}
