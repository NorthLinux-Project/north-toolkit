#version 450

layout(set = 0, binding = 0) uniform sampler2D u_atlas;

layout(location = 0) in vec2 v_uv;
layout(location = 1) in vec4 v_color;

layout(location = 0) out vec4 out_color;

void main() {
    float raw_coverage = texture(u_atlas, v_uv).r;
    if (raw_coverage <= 0.001) {
        discard;
    }

    float lum = dot(v_color.rgb, vec3(0.2126, 0.7152, 0.0722));
    float gamma_factor = mix(1.15, 1.0 / 1.15, lum);
    float corrected_alpha = pow(raw_coverage, gamma_factor);

    out_color = vec4(v_color.rgb, v_color.a * corrected_alpha);
}
