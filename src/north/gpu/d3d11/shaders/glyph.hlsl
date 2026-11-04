cbuffer ViewConstants : register(b0) {
    float2 u_viewport;
    float2 u_pad;
};

Texture2D g_atlas_texture : register(t0);
SamplerState g_atlas_sampler : register(s0);

struct VSInput {
    uint vertex_id : SV_VertexID;
    float4 dst_rect : DST_RECT;
    float4 uv_rect : UV_RECT;
    float4 color : COLOR;
    float4 clip_rect : CLIP_RECT;
};

struct PSInput {
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
    float4 color : COLOR;
    float4 clip_rect : CLIP_RECT;
    float2 global_pos : GLOBAL_POS;
};

PSInput vs_main(VSInput input) {
    PSInput output;
    float2 unit_uv = float2((input.vertex_id & 1) ? 1.0 : 0.0, (input.vertex_id & 2) ? 1.0 : 0.0);
    float2 quad_pos = input.dst_rect.xy + unit_uv * input.dst_rect.zw;

    float2 ndc = (quad_pos / u_viewport) * float2(2.0, -2.0) + float2(-1.0, 1.0);
    output.pos = float4(ndc, 0.0, 1.0);
    output.global_pos = quad_pos;
    output.uv = input.uv_rect.xy + unit_uv * input.uv_rect.zw;
    output.color = input.color;
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

    float alpha = g_atlas_texture.Sample(g_atlas_sampler, input.uv).r;
    float4 result = input.color * alpha;

    if (result.a <= 0.001) {
        discard;
    }
    return result;
}
