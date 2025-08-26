/* Windows-only lightweight PNG loader using WIC (avoids SDL_image dependency). */
#ifdef _WIN32
#include "png_wic.h"
#include "../util/log.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wincodec.h>
#include <windows.h>

// Very small, static hash set to warn-once per unique image path.
static uint32_t rogue__wic_warn_hashes[256];
static int rogue__wic_warn_count = 0;

static uint32_t rogue__hash_path_ci(const char* s)
{
    // djb2 variant, case-insensitive to avoid duplicates differing only by case
    uint32_t h = 5381u;
    for (unsigned char c; (c = (unsigned char) *s++) != 0;)
    {
        if (c >= 'A' && c <= 'Z')
            c = (unsigned char) (c + 32); // tolower ASCII
        h = ((h << 5) + h) + c;
    }
    if (h == 0u)
        h = 1u; // reserve 0 as empty sentinel
    return h;
}

static int rogue__wic_should_log_once(const char* path)
{
    uint32_t h = rogue__hash_path_ci(path);
    // linear probe in tiny table; acceptable for small N
    for (int i = 0; i < rogue__wic_warn_count; ++i)
    {
        if (rogue__wic_warn_hashes[i] == h)
            return 0; // seen already, suppress
    }
    if (rogue__wic_warn_count <
        (int) (sizeof(rogue__wic_warn_hashes) / sizeof(rogue__wic_warn_hashes[0])))
    {
        rogue__wic_warn_hashes[rogue__wic_warn_count++] = h;
    }
    return 1; // first time, log it
}

bool rogue_png_load_rgba(const char* path, unsigned char** out_pixels, int* w, int* h)
{
    if (!out_pixels || !w || !h)
        return false;
    *out_pixels = NULL;
    *w = *h = 0;
    static int s_com_inited = 0;
    if (!s_com_inited)
    {
        HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
        (void) hr;
        s_com_inited = 1;
    }
    IWICImagingFactory* factory = NULL;
    HRESULT hr = CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
                                  &IID_IWICImagingFactory, (LPVOID*) &factory);
    if (FAILED(hr) || !factory)
    {
        ROGUE_LOG_WARN("WIC factory create failed");
        return false;
    }
    int wlen = MultiByteToWideChar(CP_UTF8, 0, path, -1, NULL, 0);
    wchar_t* wpath = (wchar_t*) malloc(sizeof(wchar_t) * wlen);
    MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, wlen);
    IWICBitmapDecoder* decoder = NULL;
    hr = factory->lpVtbl->CreateDecoderFromFilename(factory, wpath, NULL, GENERIC_READ,
                                                    WICDecodeMetadataCacheOnDemand, &decoder);
    free(wpath);
    if (FAILED(hr) || !decoder)
    {
        if (rogue__wic_should_log_once(path))
            ROGUE_LOG_WARN("WIC decoder open failed: %s", path);
        factory->lpVtbl->Release(factory);
        return false;
    }
    IWICBitmapFrameDecode* frame = NULL;
    hr = decoder->lpVtbl->GetFrame(decoder, 0, &frame);
    if (FAILED(hr) || !frame)
    {
        if (rogue__wic_should_log_once(path))
            ROGUE_LOG_WARN("WIC get frame failed: %s", path);
        decoder->lpVtbl->Release(decoder);
        factory->lpVtbl->Release(factory);
        return false;
    }
    IWICFormatConverter* converter = NULL;
    hr = factory->lpVtbl->CreateFormatConverter(factory, &converter);
    if (FAILED(hr) || !converter)
    {
        if (rogue__wic_should_log_once(path))
            ROGUE_LOG_WARN("WIC converter create failed: %s", path);
        frame->lpVtbl->Release(frame);
        decoder->lpVtbl->Release(decoder);
        factory->lpVtbl->Release(factory);
        return false;
    }
    hr = converter->lpVtbl->Initialize(converter, (IWICBitmapSource*) frame,
                                       &GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, NULL,
                                       0.0, WICBitmapPaletteTypeCustom);
    if (FAILED(hr))
    {
        if (rogue__wic_should_log_once(path))
            ROGUE_LOG_WARN("WIC converter init failed: %s", path);
        converter->lpVtbl->Release(converter);
        frame->lpVtbl->Release(frame);
        decoder->lpVtbl->Release(decoder);
        factory->lpVtbl->Release(factory);
        return false;
    }
    UINT width = 0, height = 0;
    converter->lpVtbl->GetSize(converter, &width, &height);
    if (width == 0 || height == 0)
    {
        converter->lpVtbl->Release(converter);
        frame->lpVtbl->Release(frame);
        decoder->lpVtbl->Release(decoder);
        factory->lpVtbl->Release(factory);
        return false;
    }
    size_t stride = width * 4ULL;
    size_t total = stride * height;
    unsigned char* buf = (unsigned char*) malloc(total);
    if (!buf)
    {
        converter->lpVtbl->Release(converter);
        frame->lpVtbl->Release(frame);
        decoder->lpVtbl->Release(decoder);
        factory->lpVtbl->Release(factory);
        return false;
    }
    hr = converter->lpVtbl->CopyPixels(converter, NULL, (UINT) stride, (UINT) total, buf);
    if (FAILED(hr))
    {
        free(buf);
        converter->lpVtbl->Release(converter);
        frame->lpVtbl->Release(frame);
        decoder->lpVtbl->Release(decoder);
        factory->lpVtbl->Release(factory);
        if (rogue__wic_should_log_once(path))
            ROGUE_LOG_WARN("WIC copy pixels failed: %s", path);
        return false;
    }
    *out_pixels = buf;
    *w = (int) width;
    *h = (int) height;
    converter->lpVtbl->Release(converter);
    frame->lpVtbl->Release(frame);
    decoder->lpVtbl->Release(decoder);
    factory->lpVtbl->Release(factory);
    return true;
}
#endif /* _WIN32 */
