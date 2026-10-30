#version 450

layout(set = 0, binding = 0) uniform sampler2D u_texture;

layout(location = 0) in vec2 v_uv;
layout(location = 1) in vec2 v_local_pos;
layout(location = 2) in vec2 v_half_size;
layout(location = 3) in vec4 v_color;
layout(location = 4) in vec4 v_border_color;
layout(location = 5) in vec4 v_radii;
layout(location = 6) in vec4 v_params;

layout(location = 0) out vec4 out_color;

float length4(vec2 p) {
    vec2 p2 = p * p;
    return sqrt(sqrt(p2.x * p2.x + p2.y * p2.y));
}

float squircle_box_sdf(vec2 p, vec2 b, vec4 radii) {
    float r = (p.y < 0.0)
        ? ((p.x < 0.0) ? radii.x : radii.y)
        : ((p.x < 0.0) ? radii.w : radii.z);
    r = min(r, min(b.x, b.y));

    if (r <= 0.001) {
        vec2 d_rect = abs(p) - b;
        return max(d_rect.x, d_rect.y);
    }

    vec2 q = abs(p) - b + vec2(r);
    return length4(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}

void main() {
    float border_width = v_params.x;
    float use_texture = v_params.y;
    float global_alpha = v_params.z;

    float dist = squircle_box_sdf(v_local_pos, v_half_size, v_radii);
    float edge_smoothing = clamp(max(fwidth(dist), 0.75), 0.75, 1.5);
    float alpha = clamp(0.5 - dist / edge_smoothing, 0.0, 1.0);

    if (alpha <= 0.001) {
        discard;
    }

    vec4 base_color = v_color;
    if (use_texture > 0.5) {
        vec4 tex_color = texture(u_texture, v_uv);
        vec3 unmultiplied_rgb = tex_color.a > 0.001 ? (tex_color.rgb / tex_color.a) : tex_color.rgb;
        base_color = vec4(base_color.rgb * unmultiplied_rgb, base_color.a * tex_color.a);
    }

    if (border_width > 0.0) {
        float inner_dist = dist + border_width;
        float border_factor = clamp(0.5 - inner_dist / edge_smoothing, 0.0, 1.0);
        base_color = mix(v_border_color, base_color, border_factor);
    }

    out_color = vec4(base_color.rgb, base_color.a * alpha * global_alpha);
}
