#include "north/text/font_manager.hpp"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <cstring>
#include <fstream>
#include <vector>

#include <dwrite.h>
#include <windows.h>

#include <hb-ft.h>
#include <wrl/client.h>

#pragma comment(lib, "dwrite.lib")

namespace north::text {

FontManager& FontManager::instance() {
    static FontManager mgr;
    return mgr;
}

FontManager::FontManager() {
    FT_Init_FreeType(&ft_lib_);
}

FontManager::~FontManager() {
    font_cache_.clear();
    if (ft_lib_) {
        FT_Done_FreeType(ft_lib_);
        ft_lib_ = nullptr;
    }
}

std::shared_ptr<Font> FontManager::load_from_memory(std::span<const uint8_t> data) {
    if (!ft_lib_ || data.empty()) {
        return nullptr;
    }

    std::vector<uint8_t> font_bytes(data.begin(), data.end());
    FT_Face face = nullptr;
    FT_Error err = FT_New_Memory_Face(ft_lib_, font_bytes.data(),
                                      static_cast<FT_Long>(font_bytes.size()), 0, &face);
    if (err != 0 || !face) {
        return nullptr;
    }

    hb_font_t* hb_font = hb_ft_font_create_referenced(face);
    if (!hb_font) {
        FT_Done_Face(face);
        return nullptr;
    }

    return std::make_shared<Font>(face, hb_font, std::move(font_bytes));
}

std::shared_ptr<Font> FontManager::load_from_file(const std::string& path) {
    auto it = font_cache_.find(path);
    if (it != font_cache_.end()) {
        return it->second;
    }

    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return nullptr;
    }
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> buffer(static_cast<size_t>(size));
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        return nullptr;
    }

    auto font = load_from_memory(buffer);
    if (font) {
        font_cache_[path] = font;
    }
    return font;
}

std::shared_ptr<Font> FontManager::load_system_font(std::string_view family_name) {
    std::string key(family_name);
    auto it = font_cache_.find(key);
    if (it != font_cache_.end()) {
        return it->second;
    }

    Microsoft::WRL::ComPtr<IDWriteFactory> factory;
    HRESULT hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                                     reinterpret_cast<IUnknown**>(factory.GetAddressOf()));

    if (SUCCEEDED(hr)) {
        Microsoft::WRL::ComPtr<IDWriteFontCollection> collection;
        hr = factory->GetSystemFontCollection(&collection, FALSE);
        if (SUCCEEDED(hr)) {
            std::wstring wname(family_name.begin(), family_name.end());
            UINT32 index = 0;
            BOOL exists = FALSE;
            hr = collection->FindFamilyName(wname.c_str(), &index, &exists);
            if (!exists || FAILED(hr)) {
                hr = collection->FindFamilyName(L"Segoe UI", &index, &exists);
                if (!exists || FAILED(hr)) {
                    index = 0;
                    exists = (collection->GetFontFamilyCount() > 0);
                }
            }

            if (exists) {
                Microsoft::WRL::ComPtr<IDWriteFontFamily> family;
                if (SUCCEEDED(collection->GetFontFamily(index, &family))) {
                    Microsoft::WRL::ComPtr<IDWriteFont> dwrite_font;
                    if (SUCCEEDED(family->GetFirstMatchingFont(
                            DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                            DWRITE_FONT_STYLE_NORMAL, &dwrite_font))) {
                        Microsoft::WRL::ComPtr<IDWriteFontFace> font_face;
                        if (SUCCEEDED(dwrite_font->CreateFontFace(&font_face))) {
                            UINT32 file_count = 0;
                            font_face->GetFiles(&file_count, nullptr);
                            if (file_count > 0) {
                                std::vector<Microsoft::WRL::ComPtr<IDWriteFontFile>> files(
                                    file_count);
                                if (SUCCEEDED(font_face->GetFiles(&file_count,
                                                                  files[0].GetAddressOf()))) {
                                    const void* ref_key = nullptr;
                                    UINT32 ref_key_size = 0;
                                    if (SUCCEEDED(
                                            files[0]->GetReferenceKey(&ref_key, &ref_key_size))) {
                                        Microsoft::WRL::ComPtr<IDWriteFontFileLoader> loader;
                                        if (SUCCEEDED(files[0]->GetLoader(&loader))) {
                                            Microsoft::WRL::ComPtr<IDWriteFontFileStream> stream;
                                            if (SUCCEEDED(loader->CreateStreamFromKey(
                                                    ref_key, ref_key_size, &stream))) {
                                                UINT64 file_size = 0;
                                                if (SUCCEEDED(stream->GetFileSize(&file_size)) &&
                                                    file_size > 0) {
                                                    const void* fragment_start = nullptr;
                                                    void* fragment_ctx = nullptr;
                                                    if (SUCCEEDED(stream->ReadFileFragment(
                                                            &fragment_start, 0, file_size,
                                                            &fragment_ctx))) {
                                                        std::vector<uint8_t> buffer(
                                                            static_cast<size_t>(file_size));
                                                        std::memcpy(buffer.data(), fragment_start,
                                                                    static_cast<size_t>(file_size));
                                                        stream->ReleaseFileFragment(fragment_ctx);

                                                        auto font = load_from_memory(buffer);
                                                        if (font) {
                                                            font_cache_[key] = font;
                                                            return font;
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    std::vector<std::string> fallback_paths = {"C:\\Windows\\Fonts\\segoeui.ttf",
                                               "C:\\Windows\\Fonts\\arial.ttf",
                                               "C:\\Windows\\Fonts\\calibri.ttf"};
    for (const auto& path : fallback_paths) {
        auto font = load_from_file(path);
        if (font) {
            font_cache_[key] = font;
            return font;
        }
    }

    return nullptr;
}

} // namespace north::text
