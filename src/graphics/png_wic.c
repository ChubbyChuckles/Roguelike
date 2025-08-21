/* Windows-only lightweight PNG loader using WIC (avoids SDL_image dependency). */
#ifdef _WIN32
#include "graphics/png_wic.h"
#include "util/log.h"
#include <stdlib.h>
#include <wincodec.h>
#include <windows.h>

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
        ROGUE_LOG_WARN("WIC decoder open failed: %s", path);
        factory->lpVtbl->Release(factory);
        return false;
    }
    IWICBitmapFrameDecode* frame = NULL;
    hr = decoder->lpVtbl->GetFrame(decoder, 0, &frame);
    if (FAILED(hr) || !frame)
    {
        ROGUE_LOG_WARN("WIC get frame failed: %s", path);
        decoder->lpVtbl->Release(decoder);
        factory->lpVtbl->Release(factory);
        return false;
    }
    IWICFormatConverter* converter = NULL;
    hr = factory->lpVtbl->CreateFormatConverter(factory, &converter);
    if (FAILED(hr) || !converter)
    {
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
