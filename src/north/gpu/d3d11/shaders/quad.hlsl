cbuffer ViewConstants : register(b0) {
    float2 u_viewport;
    float2 u_pad;
};

struct VSInput {
    uint vertex_id : SV_VertexID;
    float4 rect : RECT;
    float4 radii : RADII;
    float4 fill_color : FILL_COLOR;
    float4 border_color : BORDER_COLOR;
    float4 border_width : BORDER_WIDTH;
    float4 clip_rect : CLIP_RECT;
    float4 shadow_color : SHADOW_COLOR;
    float4 shadow_params : SHADOW_PARAMS; // radius, spread, offset_x, offset_y
};

struct PSInput {
    float4 pos : SV_Position;
    float2 local_pos : LOCAL_POS;
    float2 rect_size : RECT_SIZE;
    float4 radii : RADII;
    float4 fill_color : FILL_COLOR;
    float4 border_color : BORDER_COLOR;
    float4 border_width : BORDER_WIDTH;
    float4 clip_rect : CLIP_RECT;
    float4 shadow_color : SHADOW_COLOR;
    float4 shadow_params : SHADOW_PARAMS;
    float2 global_pos : GLOBAL_POS;
};

PSInput vs_main(VSInput input) {
    PSInput output;

    // Unit quad UV: (0,0), (1,0), (0,1), (1,1) for triangle strip
    float2 uv = float2((input.vertex_id & 1) ? 1.0 : 0.0, (input.vertex_id & 2) ? 1.0 : 0.0);

    float blur = input.shadow_params.x;
    float spread = input.shadow_params.y;
    float2 shadow_offset = input.shadow_params.zw;
    float margin = max(blur * 2.0 + spread, 2.0);

    float2 min_pos = input.rect.xy - float2(margin, margin) + min(float2(0.0, 0.0), shadow_offset);
    float2 max_pos = input.rect.xy + input.rect.zw + float2(margin, margin) + max(float2(0.0, 0.0), shadow_offset);

    float2 quad_pos = lerp(min_pos, max_pos, uv);

    // Convert pixel coordinates to NDC [-1, 1] with Y flipped for DirectX
    float2 ndc = (quad_pos / u_viewport) * float2(2.0, -2.0) + float2(-1.0, 1.0);
    output.pos = float4(ndc, 0.0, 1.0);

    output.global_pos = quad_pos;
    output.local_pos = quad_pos - input.rect.xy;
    output.rect_size = input.rect.zw;
    output.radii = input.radii;
    output.fill_color = input.fill_color;
    output.border_color = input.border_color;
    output.border_width = input.border_width;
    output.clip_rect = input.clip_rect;
    output.shadow_color = input.shadow_color;
    output.shadow_params = input.shadow_params;

    return output;
}

// Evan Wallace rational approximation for erf(x)
float erf_approx(float x) {
    float s = sign(x);
    float a = abs(x);
    float t = 1.0 / (1.0 + 0.3275911 * a);
    float y = 1.0 - (((((1.061405429 * t - 1.453152027) * t) + 1.421413741) * t - 0.284496736) * t + 0.254829592) * t * exp(-a * a);
    return s * y;
}

float gaussian_box_1d(float p0, float p1, float sigma) {
    float inv_sigma_sqrt2 = 1.0 / (sigma * 1.41421356);
    return 0.5 * (erf_approx(p1 * inv_sigma_sqrt2) - erf_approx(p0 * inv_sigma_sqrt2));
}

float4 ps_main(PSInput input) : SV_Target {
    // Scissor clipping
    if (input.global_pos.x < input.clip_rect.x ||
        input.global_pos.x > (input.clip_rect.x + input.clip_rect.z) ||
        input.global_pos.y < input.clip_rect.y ||
        input.global_pos.y > (input.clip_rect.y + input.clip_rect.w)) {
        discard;
    }

    // SDF calculation for rounded rect
    float2 half_size = input.rect_size * 0.5;
    float2 p = input.local_pos - half_size;

    float r = (p.x > 0.0) ? ((p.y > 0.0) ? input.radii.z : input.radii.y)
                          : ((p.y > 0.0) ? input.radii.w : input.radii.x);
    r = min(r, min(half_size.x, half_size.y));

    float2 q = abs(p) - half_size + float2(r, r);
    float dist = length(max(q, float2(0.0, 0.0))) + min(max(q.x, q.y), 0.0) - r;

    float aa = max(length(float2(ddx(dist), ddy(dist))), 0.001);
    float fill_alpha = saturate(0.5 - dist / aa);

    // Box shadow calculation
    float4 shadow = float4(0.0, 0.0, 0.0, 0.0);
    float shadow_radius = input.shadow_params.x;
    if (shadow_radius > 0.0 && input.shadow_color.a > 0.0) {
        float2 shadow_pos = input.local_pos - input.shadow_params.zw;
        float sigma = max(shadow_radius * 0.5, 0.001);
        float gx = gaussian_box_1d(shadow_pos.x - input.rect_size.x, shadow_pos.x, sigma);
        float gy = gaussian_box_1d(shadow_pos.y - input.rect_size.y, shadow_pos.y, sigma);
        float shadow_alpha = saturate(gx * gy);
        shadow = input.shadow_color * shadow_alpha;
    }

    // Border and Fill composition (premultiplied alpha)
    float4 body_color;
    float min_border = min(input.border_width.x, min(input.border_width.y, min(input.border_width.z, input.border_width.w)));
    if (min_border > 0.0 && input.border_color.a > 0.0) {
        float inner_dist = dist + min_border;
        float inner_alpha = saturate(0.5 - inner_dist / aa);
        float border_alpha = saturate(fill_alpha - inner_alpha);
        body_color = input.fill_color * inner_alpha + input.border_color * border_alpha;
    } else {
        body_color = input.fill_color * fill_alpha;
    }

    // Composite body over shadow
    float4 result_color = body_color + shadow * (1.0 - body_color.a);

    if (result_color.a <= 0.001) {
        discard;
    }

    return result_color;
}
