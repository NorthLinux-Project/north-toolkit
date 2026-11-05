#include "north/render/svg_path.hpp"

#include <cctype>
#include <charconv>

namespace north::render {

namespace {

class SvgPathReader {
public:
    explicit SvgPathReader(std::string_view str) noexcept : str_(str), pos_(0) {}

    [[nodiscard]] bool has_more() const noexcept {
        skip_ws_and_comma();
        return pos_ < str_.size();
    }

    void skip_ws_and_comma() const noexcept {
        while (pos_ < str_.size()) {
            char c = str_[pos_];
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == ',') {
                ++pos_;
            } else {
                break;
            }
        }
    }

    [[nodiscard]] bool peek_is_command() const noexcept {
        skip_ws_and_comma();
        if (pos_ >= str_.size()) return false;
        char c = str_[pos_];
        return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
    }

    [[nodiscard]] char read_command() noexcept {
        skip_ws_and_comma();
        if (pos_ >= str_.size()) return '\0';
        return str_[pos_++];
    }

    [[nodiscard]] bool read_float(float& out_val) noexcept {
        skip_ws_and_comma();
        if (pos_ >= str_.size()) return false;

        const char* start = str_.data() + pos_;
        const char* end = str_.data() + str_.size();

        float val = 0.0f;
        auto [ptr, ec] = std::from_chars(start, end, val);
        if (ec != std::errc{}) {
            return false;
        }

        pos_ += static_cast<size_t>(ptr - start);
        out_val = val;
        return true;
    }

    [[nodiscard]] bool read_flag(bool& out_flag) noexcept {
        skip_ws_and_comma();
        if (pos_ >= str_.size()) return false;
        char c = str_[pos_];
        if (c == '0') {
            out_flag = false;
            ++pos_;
            return true;
        }
        if (c == '1') {
            out_flag = true;
            ++pos_;
            return true;
        }
        return false;
    }

private:
    std::string_view str_;
    mutable size_t pos_{0};
};

} // namespace

bool parse_svg_path(std::string_view d, Path& out_path) noexcept {
    SvgPathReader reader(d);

    char cmd = '\0';
    char prev_cmd = '\0';
    core::Point cur{0.0f, 0.0f};
    core::Point last_ctrl{0.0f, 0.0f};

    while (reader.has_more()) {
        if (reader.peek_is_command()) {
            cmd = reader.read_command();
        } else if (cmd == '\0') {
            return false;
        } else {
            // Implicit repeat command:
            // If previous was 'M', repeated coordinates become 'L'
            // If previous was 'm', repeated coordinates become 'l'
            if (cmd == 'M') cmd = 'L';
            else if (cmd == 'm') cmd = 'l';
        }

        switch (cmd) {
        case 'M':
        case 'm': {
            float x = 0.0f;
            float y = 0.0f;
            if (!reader.read_float(x) || !reader.read_float(y)) return false;
            if (cmd == 'm') {
                cur.x += x;
                cur.y += y;
            } else {
                cur.x = x;
                cur.y = y;
            }
            out_path.move_to(cur);
            last_ctrl = cur;
            prev_cmd = cmd;
            break;
        }
        case 'L':
        case 'l': {
            float x = 0.0f;
            float y = 0.0f;
            if (!reader.read_float(x) || !reader.read_float(y)) return false;
            if (cmd == 'l') {
                cur.x += x;
                cur.y += y;
            } else {
                cur.x = x;
                cur.y = y;
            }
            out_path.line_to(cur);
            last_ctrl = cur;
            prev_cmd = cmd;
            break;
        }
        case 'H':
        case 'h': {
            float x = 0.0f;
            if (!reader.read_float(x)) return false;
            if (cmd == 'h') {
                cur.x += x;
            } else {
                cur.x = x;
            }
            out_path.line_to(cur);
            last_ctrl = cur;
            prev_cmd = cmd;
            break;
        }
        case 'V':
        case 'v': {
            float y = 0.0f;
            if (!reader.read_float(y)) return false;
            if (cmd == 'v') {
                cur.y += y;
            } else {
                cur.y = y;
            }
            out_path.line_to(cur);
            last_ctrl = cur;
            prev_cmd = cmd;
            break;
        }
        case 'C':
        case 'c': {
            float x1 = 0.0f;
            float y1 = 0.0f;
            float x2 = 0.0f;
            float y2 = 0.0f;
            float x = 0.0f;
            float y = 0.0f;
            if (!reader.read_float(x1) || !reader.read_float(y1) ||
                !reader.read_float(x2) || !reader.read_float(y2) ||
                !reader.read_float(x) || !reader.read_float(y)) {
                return false;
            }
            core::Point c1 = (cmd == 'c') ? core::Point{cur.x + x1, cur.y + y1} : core::Point{x1, y1};
            core::Point c2 = (cmd == 'c') ? core::Point{cur.x + x2, cur.y + y2} : core::Point{x2, y2};
            core::Point to = (cmd == 'c') ? core::Point{cur.x + x, cur.y + y} : core::Point{x, y};

            out_path.cubic_to(c1, c2, to);
            last_ctrl = c2;
            cur = to;
            prev_cmd = cmd;
            break;
        }
        case 'S':
        case 's': {
            float x2 = 0.0f;
            float y2 = 0.0f;
            float x = 0.0f;
            float y = 0.0f;
            if (!reader.read_float(x2) || !reader.read_float(y2) ||
                !reader.read_float(x) || !reader.read_float(y)) {
                return false;
            }
            core::Point c1 = cur;
            if (prev_cmd == 'C' || prev_cmd == 'c' || prev_cmd == 'S' || prev_cmd == 's') {
                c1 = core::Point{2.0f * cur.x - last_ctrl.x, 2.0f * cur.y - last_ctrl.y};
            }
            core::Point c2 = (cmd == 's') ? core::Point{cur.x + x2, cur.y + y2} : core::Point{x2, y2};
            core::Point to = (cmd == 's') ? core::Point{cur.x + x, cur.y + y} : core::Point{x, y};

            out_path.cubic_to(c1, c2, to);
            last_ctrl = c2;
            cur = to;
            prev_cmd = cmd;
            break;
        }
        case 'Q':
        case 'q': {
            float x1 = 0.0f;
            float y1 = 0.0f;
            float x = 0.0f;
            float y = 0.0f;
            if (!reader.read_float(x1) || !reader.read_float(y1) ||
                !reader.read_float(x) || !reader.read_float(y)) {
                return false;
            }
            core::Point c = (cmd == 'q') ? core::Point{cur.x + x1, cur.y + y1} : core::Point{x1, y1};
            core::Point to = (cmd == 'q') ? core::Point{cur.x + x, cur.y + y} : core::Point{x, y};

            out_path.quad_to(c, to);
            last_ctrl = c;
            cur = to;
            prev_cmd = cmd;
            break;
        }
        case 'T':
        case 't': {
            float x = 0.0f;
            float y = 0.0f;
            if (!reader.read_float(x) || !reader.read_float(y)) {
                return false;
            }
            core::Point c = cur;
            if (prev_cmd == 'Q' || prev_cmd == 'q' || prev_cmd == 'T' || prev_cmd == 't') {
                c = core::Point{2.0f * cur.x - last_ctrl.x, 2.0f * cur.y - last_ctrl.y};
            }
            core::Point to = (cmd == 't') ? core::Point{cur.x + x, cur.y + y} : core::Point{x, y};

            out_path.quad_to(c, to);
            last_ctrl = c;
            cur = to;
            prev_cmd = cmd;
            break;
        }
        case 'A':
        case 'a': {
            float rx = 0.0f;
            float ry = 0.0f;
            float rot = 0.0f;
            bool large_arc = false;
            bool sweep = false;
            float x = 0.0f;
            float y = 0.0f;

            if (!reader.read_float(rx) || !reader.read_float(ry) || !reader.read_float(rot) ||
                !reader.read_flag(large_arc) || !reader.read_flag(sweep) ||
                !reader.read_float(x) || !reader.read_float(y)) {
                return false;
            }

            core::Point to = (cmd == 'a') ? core::Point{cur.x + x, cur.y + y} : core::Point{x, y};
            out_path.arc_to(to, core::Size{rx, ry}, rot, large_arc, sweep);
            cur = to;
            last_ctrl = cur;
            prev_cmd = cmd;
            break;
        }
        case 'Z':
        case 'z': {
            out_path.close();
            cur = out_path.current_point();
            last_ctrl = cur;
            prev_cmd = cmd;
            break;
        }
        default:
            return false;
        }
    }

    return true;
}

} // namespace north::render
