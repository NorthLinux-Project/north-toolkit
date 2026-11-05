cbuffer ViewConstants : register(b0) {
    float2 u_viewport;
    float2 u_pad;
};

Texture2D u_texture : register(t0);
SamplerState u_sampler : register(s0);

struct VSInput {
    uint vertex_id : SV_VertexID;
    float4 rect : RECT;
    float4 uv_rect : UV_RECT;
    float4 radii : RADII;
    float4 tint_color : TINT_COLOR;
    float4 border_color : BORDER_COLOR;
    float4 border_width : BORDER_WIDTH;
    float4 clip_rect : CLIP_RECT;
};

struct PSInput {
    float4 pos : SV_Position;
    float2 local_pos : LOCAL_POS;
    float2 rect_size : RECT_SIZE;
    float2 tex_uv : TEX_UV;
    float4 radii : RADII;
    float4 tint_color : TINT_COLOR;
    float4 border_color : BORDER_COLOR;
    float4 border_width : BORDER_WIDTH;
    float4 clip_rect : CLIP_RECT;
    float2 global_pos : GLOBAL_POS;
};

PSInput vs_main(VSInput input) {
    PSInput output;

    float2 uv = float2((input.vertex_id & 1) ? 1.0 : 0.0, (input.vertex_id & 2) ? 1.0 : 0.0);

    float margin = 1.0;
    float2 min_pos = input.rect.xy - float2(margin, margin);
    float2 max_pos = input.rect.xy + input.rect.zw + float2(margin, margin);
    float2 quad_pos = lerp(min_pos, max_pos, uv);

    float2 ndc = (quad_pos / u_viewport) * float2(2.0, -2.0) + float2(-1.0, 1.0);
    output.pos = float4(ndc, 0.0, 1.0);

    output.global_pos = quad_pos;
    output.local_pos = quad_pos - input.rect.xy;
    output.rect_size = input.rect.zw;

    float2 normalized_local = (quad_pos - input.rect.xy) / input.rect.zw;
    output.tex_uv = lerp(input.uv_rect.xy, input.uv_rect.zw, normalized_local);

    output.radii = input.radii;
    output.tint_color = input.tint_color;
    output.border_color = input.border_color;
    output.border_width = input.border_width;
    output.clip_rect = input.clip_rect;

    return output;
}

float4 ps_main(PSInput input) : SV_Target {
    if (input.global_pos.x < input.clip_rect.x ||
        input.global_pos.x > (input.clip_rect.x + input.clip_rect.z) ||
        input.global_pos.y < input.clip_rect.y ||
        input.global_pos.y > (input.clip_rect.y + input.clip_rect.w)) {
        discard;
    }

    float2 half_size = input.rect_size * 0.5;
    float2 p = input.local_pos - half_size;

    float r = (p.x > 0.0) ? ((p.y > 0.0) ? input.radii.z : input.radii.y)
                          : ((p.y > 0.0) ? input.radii.w : input.radii.x);
    r = min(r, min(half_size.x, half_size.y));

    float2 q = abs(p) - half_size + float2(r, r);
    float dist = length(max(q, float2(0.0, 0.0))) + min(max(q.x, q.y), 0.0) - r;

    float aa = max(length(float2(ddx(dist), ddy(dist))), 0.001);
    float fill_alpha = saturate(0.5 - dist / aa);

    if (fill_alpha <= 0.001) {
        discard;
    }

    float4 tex_sample = u_texture.Sample(u_sampler, input.tex_uv);
    float4 sampled_color = tex_sample * input.tint_color;

    float min_border = min(input.border_width.x, min(input.border_width.y, min(input.border_width.z, input.border_width.w)));
    float4 result_color;
    if (min_border > 0.0 && input.border_color.a > 0.0) {
        float inner_dist = dist + min_border;
        float inner_alpha = saturate(0.5 - inner_dist / aa);
        float border_alpha = saturate(fill_alpha - inner_alpha);
        result_color = sampled_color * inner_alpha + input.border_color * border_alpha;
    } else {
        result_color = sampled_color * fill_alpha;
    }

    if (result_color.a <= 0.001) {
        discard;
    }

    return result_color;
}
