#version 450

layout(push_constant) uniform PushConstants {
    vec4 projection;
    vec4 rect;
    vec4 uv_rect;
    vec4 color;
    vec4 border_color;
    vec4 radii;
    vec4 params;
} pc;

layout(location = 0) out vec2 v_uv;
layout(location = 1) out vec2 v_local_pos;
layout(location = 2) out vec2 v_half_size;
layout(location = 3) out vec4 v_color;
layout(location = 4) out vec4 v_border_color;
layout(location = 5) out vec4 v_radii;
layout(location = 6) out vec4 v_params;

void main() {
    vec2 pos[6] = vec2[](
        vec2(0.0, 0.0),
        vec2(1.0, 0.0),
        vec2(0.0, 1.0),
        vec2(0.0, 1.0),
        vec2(1.0, 0.0),
        vec2(1.0, 1.0)
    );

    vec2 p = pos[gl_VertexIndex];
    vec2 screen_pos = pc.rect.xy + p * pc.rect.zw;
    gl_Position = vec4(screen_pos * pc.projection.xy + pc.projection.zw, 0.0, 1.0);

    v_uv = mix(pc.uv_rect.xy, pc.uv_rect.zw, p);
    v_half_size = pc.rect.zw * 0.5;
    v_local_pos = (p - 0.5) * pc.rect.zw;
    v_color = pc.color;
    v_border_color = pc.border_color;
    v_radii = pc.radii;
    v_params = pc.params;
}
