#include "north/image/image_decoder.hpp"

#include <string>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>

#pragma comment(lib, "windowscodecs.lib")

namespace north::image {

namespace {

using Microsoft::WRL::ComPtr;

ComPtr<IWICImagingFactory> get_wic_factory() {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    ComPtr<IWICImagingFactory> factory;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&factory));
    if (FAILED(hr)) {
        return nullptr;
    }
    return factory;
}

std::optional<DecodedImage> decode_from_decoder(IWICBitmapDecoder* decoder,
                                                IWICImagingFactory* factory) {
    if (!decoder || !factory) {
        return std::nullopt;
    }

    ComPtr<IWICBitmapFrameDecode> frame;
    HRESULT hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr)) {
        return std::nullopt;
    }

    ComPtr<IWICFormatConverter> converter;
    hr = factory->CreateFormatConverter(&converter);
    if (FAILED(hr)) {
        return std::nullopt;
    }

    hr = converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA,
                               WICBitmapDitherTypeNone, nullptr, 0.0,
                               WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) {
        return std::nullopt;
    }

    UINT width = 0;
    UINT height = 0;
    hr = converter->GetSize(&width, &height);
    if (FAILED(hr) || width == 0 || height == 0) {
        return std::nullopt;
    }

    UINT stride = width * 4;
    UINT buffer_size = stride * height;
    std::vector<uint8_t> pixels(buffer_size);

    hr = converter->CopyPixels(nullptr, stride, buffer_size, pixels.data());
    if (FAILED(hr)) {
        return std::nullopt;
    }

    return DecodedImage{
        .width = width,
        .height = height,
        .pixels = std::move(pixels),
    };
}

} // namespace

std::optional<DecodedImage> decode_image_file(std::string_view file_path) {
    if (file_path.empty()) {
        return std::nullopt;
    }

    auto factory = get_wic_factory();
    if (!factory) {
        return std::nullopt;
    }

    int wide_len = MultiByteToWideChar(CP_UTF8, 0, file_path.data(),
                                       static_cast<int>(file_path.size()), nullptr, 0);
    if (wide_len <= 0) {
        return std::nullopt;
    }

    std::wstring wide_path(static_cast<size_t>(wide_len), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, file_path.data(), static_cast<int>(file_path.size()),
                        wide_path.data(), wide_len);

    ComPtr<IWICBitmapDecoder> decoder;
    HRESULT hr = factory->CreateDecoderFromFilename(
        wide_path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder);
    if (FAILED(hr)) {
        return std::nullopt;
    }

    return decode_from_decoder(decoder.Get(), factory.Get());
}

std::optional<DecodedImage> decode_image_memory(std::span<const uint8_t> bytes) {
    if (bytes.empty()) {
        return std::nullopt;
    }

    auto factory = get_wic_factory();
    if (!factory) {
        return std::nullopt;
    }

    ComPtr<IWICStream> stream;
    HRESULT hr = factory->CreateStream(&stream);
    if (FAILED(hr)) {
        return std::nullopt;
    }

    hr = stream->InitializeFromMemory(const_cast<BYTE*>(bytes.data()),
                                      static_cast<DWORD>(bytes.size()));
    if (FAILED(hr)) {
        return std::nullopt;
    }

    ComPtr<IWICBitmapDecoder> decoder;
    hr = factory->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnDemand,
                                          &decoder);
    if (FAILED(hr)) {
        return std::nullopt;
    }

    return decode_from_decoder(decoder.Get(), factory.Get());
}

} // namespace north::image
