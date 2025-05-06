#version 330 core

in vec2 TexCoords;
out vec4 FragColor;

uniform sampler2D tex;
uniform vec2 viewportSize;

int x0[25] = int[25](
    1,2,3,2,1,
    2,3,4,3,2,
    3,4,5,4,3,
    2,3,4,3,2,
    1,2,3,3,1
);

void main() {
    vec3 r0 = vec3(0);

    r0.xy = viewportSize * TexCoords;
    r0.x = r0.x * 231.0 + r0.y;
    r0.z = 3.90015626 * r0.x;
    r0.yz = sin(r0.xz);

    vec2 center = 0.5 * (r0.y + r0.z + cos(r0.x * 1.114 + 3.7)) / viewportSize;

    vec4 color = vec4(0);
    int sum = 0;

    for(int i = -2; i <= 2; i++) {
        for(int j = -2; j <= 2; j++) {
            vec4 r5 = texture(tex, vec2(j, i) * center + TexCoords);
            if(r5.xyz != vec3(0.0, 0.0, 0.0)) {
                color = max(r5, color);
                sum += x0[i * 5 + 10 + j + 2];
            }
        }
    }

    if(4 < sum) {
        FragColor = color;
    } else {
        FragColor = vec4(0.0, 0.0, 0.0, 0.0);
    }
}
