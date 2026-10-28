#version 450

layout(push_constant) uniform GlyphPushConstants {
    mat4 projection;
} pc;

layout(location = 0) in vec4 i_rect;
layout(location = 1) in vec4 i_uv_rect;
layout(location = 2) in vec4 i_color;

layout(location = 0) out vec2 v_uv;
layout(location = 1) out vec4 v_color;

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
    vec2 screen_pos = i_rect.xy + p * i_rect.zw;
    gl_Position = pc.projection * vec4(screen_pos, 0.0, 1.0);
    v_uv = mix(i_uv_rect.xy, i_uv_rect.zw, p);
    v_color = i_color;
}
