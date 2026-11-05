#include "north/platform/clipboard.hpp"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <vector>

namespace north::platform {

bool Clipboard::set_text(std::string_view text) {
    if (text.empty()) {
        if (!OpenClipboard(nullptr)) {
            return false;
        }
        EmptyClipboard();
        CloseClipboard();
        return true;
    }

    int wlen = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (wlen <= 0) {
        return false;
    }

    HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, static_cast<size_t>(wlen + 1) * sizeof(wchar_t));
    if (!mem) {
        return false;
    }

    auto* dst = static_cast<wchar_t*>(GlobalLock(mem));
    if (!dst) {
        GlobalFree(mem);
        return false;
    }

    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), dst, wlen);
    dst[wlen] = L'\0';
    GlobalUnlock(mem);

    if (!OpenClipboard(nullptr)) {
        GlobalFree(mem);
        return false;
    }

    EmptyClipboard();
    if (!SetClipboardData(CF_UNICODETEXT, mem)) {
        GlobalFree(mem);
        CloseClipboard();
        return false;
    }

    CloseClipboard();
    return true;
}

std::string Clipboard::get_text() {
    if (!OpenClipboard(nullptr)) {
        return {};
    }

    HANDLE h_data = GetClipboardData(CF_UNICODETEXT);
    if (!h_data) {
        CloseClipboard();
        return {};
    }

    const auto* wstr = static_cast<const wchar_t*>(GlobalLock(h_data));
    if (!wstr) {
        CloseClipboard();
        return {};
    }

    int len = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
    if (len <= 1) {
        GlobalUnlock(h_data);
        CloseClipboard();
        return {};
    }

    std::string result(static_cast<size_t>(len - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, result.data(), len, nullptr, nullptr);

    GlobalUnlock(h_data);
    CloseClipboard();
    return result;
}

bool Clipboard::has_text() {
    return IsClipboardFormatAvailable(CF_UNICODETEXT) != FALSE;
}

} // namespace north::platform
