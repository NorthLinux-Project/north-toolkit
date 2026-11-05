cbuffer ViewConstants : register(b0) {
    float2 u_viewport;
    float2 u_pad;
};

struct VSInput {
    float2 pos : POSITION;
    float aa_coverage : AA_COVERAGE;
    float4 color : COLOR;
    float4 clip_rect : CLIP_RECT;
};

struct PSInput {
    float4 pos : SV_Position;
    float aa_coverage : AA_COVERAGE;
    float4 color : COLOR;
    float4 clip_rect : CLIP_RECT;
    float2 global_pos : GLOBAL_POS;
};

PSInput vs_main(VSInput input) {
    PSInput output;
    float2 ndc = (input.pos / u_viewport) * float2(2.0, -2.0) + float2(-1.0, 1.0);
    output.pos = float4(ndc, 0.0, 1.0);
    output.global_pos = input.pos;
    output.aa_coverage = input.aa_coverage;
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

    float4 result = input.color * input.aa_coverage;
    if (result.a <= 0.001) {
        discard;
    }
    return result;
}
