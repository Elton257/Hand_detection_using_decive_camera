#include "Camera.hpp"

#include <mfapi.h>
#include <mferror.h>

#include <cmath>
#include <cstdlib>
#include <cstring>

#ifdef _MSC_VER
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "ole32.lib")
#endif

static const DWORD kStream = (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM;

bool Camera::open() {
    close();

    ComPtr<IMFAttributes> attr;
    if (FAILED(MFCreateAttributes(attr.put(), 1))) return false;
    attr->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);

    IMFActivate** devs = nullptr;
    UINT32 count = 0;
    HRESULT hr = MFEnumDeviceSources(attr.get(), &devs, &count);
    if (FAILED(hr) || count == 0) { if (devs) CoTaskMemFree(devs); return false; }
    hr = devs[0]->ActivateObject(IID_PPV_ARGS(source_.put()));
    for (UINT32 i = 0; i < count; ++i) devs[i]->Release();
    CoTaskMemFree(devs);
    if (FAILED(hr)) { close(); return false; }

    ComPtr<IMFAttributes> rattr;
    if (FAILED(MFCreateAttributes(rattr.put(), 1))) { close(); return false; }
    // Let Windows convert YUY2/NV12/MJPG -> RGB32 by itself
    rattr->SetUINT32(MF_SOURCE_READER_ENABLE_ADVANCED_VIDEO_PROCESSING, TRUE);
    if (FAILED(MFCreateSourceReaderFromMediaSource(source_.get(), rattr.get(), reader_.put()))) { close(); return false; }

    reader_->SetStreamSelection((DWORD)MF_SOURCE_READER_ALL_STREAMS, FALSE);
    reader_->SetStreamSelection(kStream, TRUE);

    // Pick the most suitable format: ~640 pixels wide, >= 25 fps
    int bestIdx = -1;
    double bestScore = -1e18;
    for (DWORD i = 0; i < 1000; ++i) {
        ComPtr<IMFMediaType> t;
        if (FAILED(reader_->GetNativeMediaType(kStream, i, t.put()))) break;
        UINT64 fs = 0, fr = 0;
        if (FAILED(t->GetUINT64(MF_MT_FRAME_SIZE, &fs))) continue;
        const UINT32 w = (UINT32)(fs >> 32), h = (UINT32)(fs & 0xFFFFFFFFu);
        double fps = 0;
        if (SUCCEEDED(t->GetUINT64(MF_MT_FRAME_RATE, &fr))) {
            const UINT32 num = (UINT32)(fr >> 32), den = (UINT32)(fr & 0xFFFFFFFFu);
            if (den) fps = (double)num / den;
        }
        GUID sub = {};
        t->GetGUID(MF_MT_SUBTYPE, &sub);
        double score = -std::fabs((double)w - 640.0);
        if (w < 320 || h < 180) score -= 100000;
        score += (fps >= 25.0) ? 300.0 : fps * 5.0;
        if (sub == MFVideoFormat_YUY2 || sub == MFVideoFormat_NV12 || sub == MFVideoFormat_RGB32) score += 40;
        if (score > bestScore) { bestScore = score; bestIdx = (int)i; }
    }
    if (bestIdx >= 0) {
        ComPtr<IMFMediaType> t;
        if (SUCCEEDED(reader_->GetNativeMediaType(kStream, (DWORD)bestIdx, t.put())))
            reader_->SetCurrentMediaType(kStream, nullptr, t.get());
    }

    ComPtr<IMFMediaType> out;
    if (FAILED(MFCreateMediaType(out.put()))) { close(); return false; }
    out->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    out->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
    if (FAILED(reader_->SetCurrentMediaType(kStream, nullptr, out.get()))) { close(); return false; }

    ComPtr<IMFMediaType> cur;
    if (FAILED(reader_->GetCurrentMediaType(kStream, cur.put()))) { close(); return false; }
    UINT64 fs = 0;
    if (FAILED(cur->GetUINT64(MF_MT_FRAME_SIZE, &fs))) { close(); return false; }
    width  = (int)(fs >> 32);
    height = (int)(fs & 0xFFFFFFFFu);
    UINT32 st = 0;
    stride_ = SUCCEEDED(cur->GetUINT32(MF_MT_DEFAULT_STRIDE, &st)) ? (LONG)st : (LONG)(width * 4);
    if (width < 64 || height < 48 || width > 8192 || height > 8192) { close(); return false; }
    return true;
}

int Camera::read(uint32_t* dst, size_t cap) {
    if (!reader_) return -1;
    DWORD idx = 0, flags = 0;
    LONGLONG ts = 0;
    ComPtr<IMFSample> sample;
    const HRESULT hr = reader_->ReadSample(kStream, 0, &idx, &flags, &ts, sample.put());
    if (FAILED(hr)) return -1;
    if (flags & (MF_SOURCE_READERF_ERROR | MF_SOURCE_READERF_ENDOFSTREAM |
                 MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED)) return -1;
    if (!sample) return 0;
    if ((size_t)width * (size_t)height > cap) return -1;

    ComPtr<IMFMediaBuffer> buf;
    if (FAILED(sample->ConvertToContiguousBuffer(buf.put()))) return 0;
    const size_t rowBytes = (size_t)width * 4;

    // Preferred path: 2D buffer (gives the exact pitch, also for bottom-up images)
    ComPtr<IMF2DBuffer> b2;
    if (SUCCEEDED(buf->QueryInterface(IID_PPV_ARGS(b2.put())))) {
        BYTE* scan = nullptr;
        LONG pitch = 0;
        if (SUCCEEDED(b2->Lock2D(&scan, &pitch))) {
            int ok = 0;
            if ((size_t)std::labs(pitch) >= rowBytes) {
                for (int y = 0; y < height; ++y)
                    std::memcpy(dst + (size_t)y * width, scan + (ptrdiff_t)y * pitch, rowBytes);
                ok = 1;
            }
            b2->Unlock2D();
            return ok;
        }
    }

    // Fallback path: plain buffer + stride from the media type
    BYTE* data = nullptr;
    DWORD maxLen = 0, curLen = 0;
    if (FAILED(buf->Lock(&data, &maxLen, &curLen))) return 0;
    int ok = 0;
    const LONG pitch = stride_ ? stride_ : (LONG)rowBytes;
    const size_t ap = (size_t)std::labs(pitch);
    if (ap >= rowBytes && (size_t)curLen >= ap * (size_t)(height - 1) + rowBytes) {
        BYTE* row0 = (pitch < 0) ? data + ap * (size_t)(height - 1) : data;
        for (int y = 0; y < height; ++y)
            std::memcpy(dst + (size_t)y * width, row0 + (ptrdiff_t)y * pitch, rowBytes);
        ok = 1;
    }
    buf->Unlock();
    return ok;
}

void Camera::close() {
    reader_.reset();
    if (source_) { source_->Shutdown(); source_.reset(); }
    width = height = 0;
}
