#include "north/color/hct.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace north::color {

namespace {

constexpr float kPi = std::numbers::pi_v<float>;

struct ViewingConditions {
    float n{0.1841865f};
    float z{1.909169f};
    float nbb{1.000494f};
    float ncb{1.000494f};
    float c{0.69f};
    float nc{1.0f};
    float fl{0.3884814f};
    float fl_root{0.7894827f};
    float aw{39.2964f};
    std::array<float, 3> rgb_d{1.021178f, 0.986308f, 0.933960f};

    static const ViewingConditions& standard() noexcept {
        static const ViewingConditions vc = []() noexcept {
            ViewingConditions v;
            constexpr float white_point[3] = {95.047f, 100.0f, 108.883f};
            constexpr float la = 11.726038f;
            constexpr float yb = 18.41865f;

            v.n = yb / white_point[1];
            v.z = 1.48f + std::sqrt(v.n);
            v.nbb = 0.725f * std::pow(1.0f / v.n, 0.2f);
            v.ncb = v.nbb;
            v.c = 0.69f;
            v.nc = 1.0f;

            float k = 1.0f / (5.0f * la + 1.0f);
            float k4 = k * k * k * k;
            float k4_f = 1.0f - k4;
            v.fl = (k4 * la) + (0.1f * k4_f * k4_f * std::cbrt(5.0f * la));
            v.fl_root = std::pow(v.fl, 0.25f);

            float d = 1.0f * (1.0f - (1.0f / 3.6f) * std::exp((-la - 42.0f) / 92.0f));
            d = std::clamp(d, 0.0f, 1.0f);

            constexpr float m16[3][3] = {
                {0.401288f, 0.650173f, -0.051461f},
                {-0.250268f, 1.204414f, 0.045854f},
                {-0.002079f, 0.048952f, 0.953127f},
            };

            float rgb_w[3] = {
                m16[0][0] * white_point[0] + m16[0][1] * white_point[1] + m16[0][2] * white_point[2],
                m16[1][0] * white_point[0] + m16[1][1] * white_point[1] + m16[1][2] * white_point[2],
                m16[2][0] * white_point[0] + m16[2][1] * white_point[1] + m16[2][2] * white_point[2],
            };

            for (int i = 0; i < 3; ++i) {
                v.rgb_d[i] = d * (100.0f / rgb_w[i]) + 1.0f - d;
            }

            float rgb_aw[3];
            for (int i = 0; i < 3; ++i) {
                float val = (v.fl * v.rgb_d[i] * rgb_w[i]) / 100.0f;
                float af = std::pow(val, 0.42f);
                rgb_aw[i] = (400.0f * af) / (af + 27.13f);
            }
            v.aw = ((40.0f * rgb_aw[0] + 20.0f * rgb_aw[1] + rgb_aw[2]) / 20.0f) * v.nbb;
            return v;
        }();
        return vc;
    }
};

float tone_from_y(float y) noexcept {
    float y_norm = y / 100.0f;
    if (y_norm > 0.00885645167f) {
        return 116.0f * std::cbrt(y_norm) - 16.0f;
    }
    return 903.296296296f * y_norm;
}

float y_from_tone(float tone) noexcept {
    if (tone > 8.0f) {
        float v = (tone + 16.0f) / 116.0f;
        return 100.0f * (v * v * v);
    }
    return 100.0f * (tone / 903.296296296f);
}

struct Cam16Result {
    float hue{0.0f};
    float chroma{0.0f};
    float j{0.0f};
};

Cam16Result cam16_from_xyz(std::array<float, 3> xyz) noexcept {
    const auto& vc = ViewingConditions::standard();

    constexpr float m16[3][3] = {
        {0.401288f, 0.650173f, -0.051461f},
        {-0.250268f, 1.204414f, 0.045854f},
        {-0.002079f, 0.048952f, 0.953127f},
    };

    float r_t = m16[0][0] * xyz[0] + m16[0][1] * xyz[1] + m16[0][2] * xyz[2];
    float g_t = m16[1][0] * xyz[0] + m16[1][1] * xyz[1] + m16[1][2] * xyz[2];
    float b_t = m16[2][0] * xyz[0] + m16[2][1] * xyz[1] + m16[2][2] * xyz[2];

    float r_d = vc.rgb_d[0] * r_t;
    float g_d = vc.rgb_d[1] * g_t;
    float b_d = vc.rgb_d[2] * b_t;

    float r_af = std::pow(vc.fl * std::abs(r_d) / 100.0f, 0.42f);
    float g_af = std::pow(vc.fl * std::abs(g_d) / 100.0f, 0.42f);
    float b_af = std::pow(vc.fl * std::abs(b_d) / 100.0f, 0.42f);

    auto sign_fn = [](float val) noexcept -> float {
        return (val > 0.0f) ? 1.0f : ((val < 0.0f) ? -1.0f : 0.0f);
    };

    float r_a = sign_fn(r_d) * 400.0f * r_af / (r_af + 27.13f);
    float g_a = sign_fn(g_d) * 400.0f * g_af / (g_af + 27.13f);
    float b_a = sign_fn(b_d) * 400.0f * b_af / (b_af + 27.13f);

    float a = (11.0f * r_a - 12.0f * g_a + b_a) / 11.0f;
    float b = (r_a + g_a - 2.0f * b_a) / 9.0f;
    float u = (20.0f * r_a + 20.0f * g_a + 21.0f * b_a) / 20.0f;
    float p2 = (40.0f * r_a + 20.0f * g_a + b_a) / 20.0f;

    float atan_rad = std::atan2(b, a);
    float hue = atan_rad * (180.0f / kPi);
    if (hue < 0.0f) {
        hue += 360.0f;
    } else if (hue >= 360.0f) {
        hue -= 360.0f;
    }

    float ac = p2 * vc.nbb;
    float j = 100.0f * std::pow(ac / vc.aw, vc.c * vc.z);

    float hue_prime = (hue < 20.14f) ? (hue + 360.0f) : hue;
    float e_hue = 0.25f * (std::cos(hue_prime * (kPi / 180.0f) + 2.0f) + 3.8f);
    float p1 = (50000.0f / 13.0f) * e_hue * vc.nc * vc.ncb;
    float t = p1 * std::hypot(a, b) / (u + 0.305f);
    float alpha = std::pow(t, 0.9f) * std::pow(1.64f - std::pow(0.29f, vc.n), 0.73f);
    float chroma = alpha * std::sqrt(j / 100.0f);

    return Cam16Result{hue, chroma, j};
}

struct Cam16HuePrecomputed {
    float p1;
    float h_cos;
    float h_sin;
};

inline Cam16HuePrecomputed precompute_hue(float hue, const ViewingConditions& vc) noexcept {
    float h_rad = hue * (kPi / 180.0f);
    float h_cos = std::cos(h_rad);
    float h_sin = std::sin(h_rad);
    float e_hue = 0.25f * (std::cos(h_rad + 2.0f) + 3.8f);
    float p1 = e_hue * (50000.0f / 13.0f) * vc.nc * vc.ncb;
    return Cam16HuePrecomputed{p1, h_cos, h_sin};
}

inline float c_comp(float val, float rgb_d_elem, float fl) noexcept {
    float v_abs = (std::min)(399.999f, std::abs(val));
    float base = (27.13f * v_abs) / (400.0f - v_abs);
    float s = (val < 0.0f) ? -1.0f : 1.0f;
    return s * (100.0f / fl) * std::pow(base, 2.3809524f) / rgb_d_elem;
}

inline float y_from_cam16_fast(float j, float chroma, const Cam16HuePrecomputed& hp,
                               const ViewingConditions& vc) noexcept {
    if (j <= 0.0f) {
        return 0.0f;
    }
    float ac = vc.aw * std::pow(j / 100.0f, 0.759114f);
    float p2 = ac / vc.nbb;
    float a = 0.0f;
    float b = 0.0f;
    if (chroma > 0.0f) {
        float alpha = chroma / std::sqrt(j / 100.0f);
        float t = std::pow(alpha * 1.132057f, 1.111111f);
        float gamma = 23.0f * (p2 + 0.305f) * t /
                      (23.0f * hp.p1 + 11.0f * t * hp.h_cos + 108.0f * t * hp.h_sin);
        a = gamma * hp.h_cos;
        b = gamma * hp.h_sin;
    }
    float r_a = (460.0f * p2 + 451.0f * a + 288.0f * b) / 1403.0f;
    float g_a = (460.0f * p2 - 891.0f * a - 261.0f * b) / 1403.0f;
    float b_a = (460.0f * p2 - 220.0f * a - 6300.0f * b) / 1403.0f;

    float r_f = c_comp(r_a, vc.rgb_d[0], vc.fl);
    float g_f = c_comp(g_a, vc.rgb_d[1], vc.fl);
    float b_f = c_comp(b_a, vc.rgb_d[2], vc.fl);

    return r_f * 0.38752654f + g_f * 0.62144744f - b_f * 0.00897398f;
}

inline std::array<float, 3> xyz_from_cam16_fast(float j, float chroma,
                                                const Cam16HuePrecomputed& hp,
                                                const ViewingConditions& vc) noexcept {
    if (j <= 0.0f) {
        return {0.0f, 0.0f, 0.0f};
    }
    float ac = vc.aw * std::pow(j / 100.0f, 0.759114f);
    float p2 = ac / vc.nbb;
    float a = 0.0f;
    float b = 0.0f;
    if (chroma > 0.0f) {
        float alpha = chroma / std::sqrt(j / 100.0f);
        float t = std::pow(alpha * 1.132057f, 1.111111f);
        float gamma = 23.0f * (p2 + 0.305f) * t /
                      (23.0f * hp.p1 + 11.0f * t * hp.h_cos + 108.0f * t * hp.h_sin);
        a = gamma * hp.h_cos;
        b = gamma * hp.h_sin;
    }
    float r_a = (460.0f * p2 + 451.0f * a + 288.0f * b) / 1403.0f;
    float g_a = (460.0f * p2 - 891.0f * a - 261.0f * b) / 1403.0f;
    float b_a = (460.0f * p2 - 220.0f * a - 6300.0f * b) / 1403.0f;

    float r_f = c_comp(r_a, vc.rgb_d[0], vc.fl);
    float g_f = c_comp(g_a, vc.rgb_d[1], vc.fl);
    float b_f = c_comp(b_a, vc.rgb_d[2], vc.fl);

    constexpr float inv_m16[3][3] = {
        {1.8620678f, -1.0112547f, 0.14918678f},
        {0.38752654f, 0.62144744f, -0.00897398f},
        {-0.01584150f, -0.03412294f, 1.0499644f},
    };

    return {
        r_f * inv_m16[0][0] + g_f * inv_m16[0][1] + b_f * inv_m16[0][2],
        r_f * inv_m16[1][0] + g_f * inv_m16[1][1] + b_f * inv_m16[1][2],
        r_f * inv_m16[2][0] + g_f * inv_m16[2][1] + b_f * inv_m16[2][2],
    };
}

std::array<float, 3> xyz_from_cam16(float j, float chroma, float hue) noexcept {
    const auto& vc = ViewingConditions::standard();
    return xyz_from_cam16_fast(j, chroma, precompute_hue(hue, vc), vc);
}

} // namespace

core::Color srgb_to_linear(core::Color c) noexcept {
    auto conv = [](float val) noexcept -> float {
        val = std::clamp(val, 0.0f, 1.0f);
        if (val <= 0.04045f) {
            return val / 12.92f;
        }
        return std::pow((val + 0.055f) / 1.055f, 2.4f);
    };
    return {conv(c.r), conv(c.g), conv(c.b), c.a};
}

core::Color linear_to_srgb(core::Color c) noexcept {
    auto conv = [](float val) noexcept -> float {
        val = std::clamp(val, 0.0f, 1.0f);
        if (val <= 0.0031308f) {
            return val * 12.92f;
        }
        return 1.055f * std::pow(val, 1.0f / 2.4f) - 0.055f;
    };
    return {std::clamp(conv(c.r), 0.0f, 1.0f),
            std::clamp(conv(c.g), 0.0f, 1.0f),
            std::clamp(conv(c.b), 0.0f, 1.0f),
            c.a};
}

std::array<float, 3> linear_rgb_to_xyz(core::Color c) noexcept {
    return {
        100.0f * (0.41233895f * c.r + 0.35762064f * c.g + 0.18051042f * c.b),
        100.0f * (0.21262013f * c.r + 0.71517030f * c.g + 0.07218056f * c.b),
        100.0f * (0.01933081f * c.r + 0.11919478f * c.g + 0.95048259f * c.b),
    };
}

core::Color xyz_to_linear_rgb(std::array<float, 3> xyz) noexcept {
    float x = xyz[0] / 100.0f;
    float y = xyz[1] / 100.0f;
    float z = xyz[2] / 100.0f;
    return {
        3.2413774792f * x - 1.5376652403f * y - 0.4988536715f * z,
        -0.9691452522f * x + 1.8758853451f * y + 0.0415658561f * z,
        0.0556209369f * x - 0.2039552456f * y + 1.0571795773f * z,
        1.0f,
    };
}

Hct xyz_to_hct(std::array<float, 3> xyz) noexcept {
    auto cam = cam16_from_xyz(xyz);
    float tone = tone_from_y(xyz[1]);
    return Hct{cam.hue, cam.chroma, std::clamp(tone, 0.0f, 100.0f)};
}

std::array<float, 3> hct_to_xyz(Hct hct) noexcept {
    float target_y = y_from_tone(std::clamp(hct.tone, 0.0f, 100.0f));
    float low = 0.0f;
    float high = 100.0f;
    float mid = 50.0f;
    for (int iter = 0; iter < 16; ++iter) {
        mid = (low + high) * 0.5f;
        auto xyz = xyz_from_cam16(mid, hct.chroma, hct.hue);
        if (xyz[1] < target_y) {
            low = mid;
        } else {
            high = mid;
        }
    }
    auto result = xyz_from_cam16(mid, hct.chroma, hct.hue);
    result[1] = target_y;
    return result;
}

Hct srgb_to_hct(core::Color c) noexcept {
    auto lin = srgb_to_linear(c);
    auto xyz = linear_rgb_to_xyz(lin);
    return xyz_to_hct(xyz);
}

core::Color hct_to_srgb(Hct hct) noexcept {
    if (hct.tone <= 0.0f) {
        return core::Color{0.0f, 0.0f, 0.0f, 1.0f};
    }
    if (hct.tone >= 100.0f) {
        return core::Color{1.0f, 1.0f, 1.0f, 1.0f};
    }

    float target_y = y_from_tone(hct.tone);
    if (hct.chroma <= 0.0001f) {
        float gray = target_y / 100.0f;
        return linear_to_srgb(core::Color{gray, gray, gray, 1.0f});
    }

    const auto& vc = ViewingConditions::standard();
    auto hp = precompute_hue(hct.hue, vc);

    auto solve_at_chroma = [&](float c) noexcept -> core::Color {
        float low = 0.0f;
        float high = 100.0f;
        float mid = 50.0f;
        for (int iter = 0; iter < 12; ++iter) {
            mid = (low + high) * 0.5f;
            float y = y_from_cam16_fast(mid, c, hp, vc);
            if (y < target_y) {
                low = mid;
            } else {
                high = mid;
            }
        }
        auto best_xyz = xyz_from_cam16_fast(mid, c, hp, vc);
        best_xyz[1] = target_y;
        return xyz_to_linear_rgb(best_xyz);
    };

    auto in_gamut = [](core::Color rgb) noexcept -> bool {
        constexpr float eps = 0.001f;
        return rgb.r >= -eps && rgb.r <= 1.0f + eps &&
               rgb.g >= -eps && rgb.g <= 1.0f + eps &&
               rgb.b >= -eps && rgb.b <= 1.0f + eps;
    };

    auto lin_rgb = solve_at_chroma(hct.chroma);
    if (in_gamut(lin_rgb)) {
        return linear_to_srgb(lin_rgb);
    }

    float low_c = 0.0f;
    float high_c = hct.chroma;
    for (int iter = 0; iter < 8; ++iter) {
        float mid_c = (low_c + high_c) * 0.5f;
        auto test_rgb = solve_at_chroma(mid_c);
        if (in_gamut(test_rgb)) {
            low_c = mid_c;
            lin_rgb = test_rgb;
        } else {
            high_c = mid_c;
        }
    }

    return linear_to_srgb(lin_rgb);
}

} // namespace north::color
