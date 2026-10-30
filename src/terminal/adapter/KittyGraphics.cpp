// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "KittyGraphics.hpp"
#include "adaptDispatch.hpp"
#include "../buffer/out/ImageSlice.hpp"
#include "../../types/inc/Viewport.hpp"

#include <wincodec.h>

#pragma comment(lib, "windowscodecs.lib")

using namespace Microsoft::Console::VirtualTerminal;

namespace
{
    // Kitty's rowcolumn-diacritics.txt, in order: index n encodes row/column n.
    constexpr std::array<char32_t, 297> diacritics{
        0x0305, 0x030D, 0x030E, 0x0310, 0x0312, 0x033D, 0x033E, 0x033F, 0x0346, 0x034A,
        0x034B, 0x034C, 0x0350, 0x0351, 0x0352, 0x0357, 0x035B, 0x0363, 0x0364, 0x0365,
        0x0366, 0x0367, 0x0368, 0x0369, 0x036A, 0x036B, 0x036C, 0x036D, 0x036E, 0x036F,
        0x0483, 0x0484, 0x0485, 0x0486, 0x0487, 0x0592, 0x0593, 0x0594, 0x0595, 0x0597,
        0x0598, 0x0599, 0x059C, 0x059D, 0x059E, 0x059F, 0x05A0, 0x05A1, 0x05A8, 0x05A9,
        0x05AB, 0x05AC, 0x05AF, 0x05C4, 0x0610, 0x0611, 0x0612, 0x0613, 0x0614, 0x0615,
        0x0616, 0x0617, 0x0657, 0x0658, 0x0659, 0x065A, 0x065B, 0x065D, 0x065E, 0x06D6,
        0x06D7, 0x06D8, 0x06D9, 0x06DA, 0x06DB, 0x06DC, 0x06DF, 0x06E0, 0x06E1, 0x06E2,
        0x06E4, 0x06E7, 0x06E8, 0x06EB, 0x06EC, 0x0730, 0x0732, 0x0733, 0x0735, 0x0736,
        0x073A, 0x073D, 0x073F, 0x0740, 0x0741, 0x0743, 0x0745, 0x0747, 0x0749, 0x074A,
        0x07EB, 0x07EC, 0x07ED, 0x07EE, 0x07EF, 0x07F0, 0x07F1, 0x07F3, 0x0816, 0x0817,
        0x0818, 0x0819, 0x081B, 0x081C, 0x081D, 0x081E, 0x081F, 0x0820, 0x0821, 0x0822,
        0x0823, 0x0825, 0x0826, 0x0827, 0x0829, 0x082A, 0x082B, 0x082C, 0x082D, 0x0951,
        0x0953, 0x0954, 0x0F82, 0x0F83, 0x0F86, 0x0F87, 0x135D, 0x135E, 0x135F, 0x17DD,
        0x193A, 0x1A17, 0x1A75, 0x1A76, 0x1A77, 0x1A78, 0x1A79, 0x1A7A, 0x1A7B, 0x1A7C,
        0x1B6B, 0x1B6D, 0x1B6E, 0x1B6F, 0x1B70, 0x1B71, 0x1B72, 0x1B73, 0x1CD0, 0x1CD1,
        0x1CD2, 0x1CDA, 0x1CDB, 0x1CE0, 0x1DC0, 0x1DC1, 0x1DC3, 0x1DC4, 0x1DC5, 0x1DC6,
        0x1DC7, 0x1DC8, 0x1DC9, 0x1DCB, 0x1DCC, 0x1DD1, 0x1DD2, 0x1DD3, 0x1DD4, 0x1DD5,
        0x1DD6, 0x1DD7, 0x1DD8, 0x1DD9, 0x1DDA, 0x1DDB, 0x1DDC, 0x1DDD, 0x1DDE, 0x1DDF,
        0x1DE0, 0x1DE1, 0x1DE2, 0x1DE3, 0x1DE4, 0x1DE5, 0x1DE6, 0x1DFE, 0x20D0, 0x20D1,
        0x20D4, 0x20D5, 0x20D6, 0x20D7, 0x20DB, 0x20DC, 0x20E1, 0x20E7, 0x20E9, 0x20F0,
        0x2CEF, 0x2CF0, 0x2CF1, 0x2DE0, 0x2DE1, 0x2DE2, 0x2DE3, 0x2DE4, 0x2DE5, 0x2DE6,
        0x2DE7, 0x2DE8, 0x2DE9, 0x2DEA, 0x2DEB, 0x2DEC, 0x2DED, 0x2DEE, 0x2DEF, 0x2DF0,
        0x2DF1, 0x2DF2, 0x2DF3, 0x2DF4, 0x2DF5, 0x2DF6, 0x2DF7, 0x2DF8, 0x2DF9, 0x2DFA,
        0x2DFB, 0x2DFC, 0x2DFD, 0x2DFE, 0x2DFF, 0xA66F, 0xA67C, 0xA67D, 0xA6F0, 0xA6F1,
        0xA8E0, 0xA8E1, 0xA8E2, 0xA8E3, 0xA8E4, 0xA8E5, 0xA8E6, 0xA8E7, 0xA8E8, 0xA8E9,
        0xA8EA, 0xA8EB, 0xA8EC, 0xA8ED, 0xA8EE, 0xA8EF, 0xA8F0, 0xA8F1, 0xAAB0, 0xAAB2,
        0xAAB3, 0xAAB7, 0xAAB8, 0xAABE, 0xAABF, 0xAAC1, 0xFE20, 0xFE21, 0xFE22, 0xFE23,
        0xFE24, 0xFE25, 0xFE26, 0x10A0F, 0x10A38, 0x1D185, 0x1D186, 0x1D187, 0x1D188, 0x1D189,
        0x1D1AA, 0x1D1AB, 0x1D1AC, 0x1D1AD, 0x1D242, 0x1D243, 0x1D244
    };

    // Images larger than this, or more stored data than this in total, are
    // refused, so a misbehaving client cannot exhaust memory.
    constexpr til::CoordType maxImageSide = 10000;
    constexpr size_t maxTotalBytes = 320 * 1024 * 1024;
    constexpr size_t maxEncodedBytes = 64 * 1024 * 1024;

    char32_t readCodePoint(const std::wstring_view text, size_t& i) noexcept
    {
        const auto lead = til::at(text, i++);
        if (lead >= 0xD800 && lead <= 0xDBFF && i < text.size())
        {
            const auto trail = til::at(text, i);
            if (trail >= 0xDC00 && trail <= 0xDFFF)
            {
                ++i;
                return 0x10000 + ((static_cast<char32_t>(lead) - 0xD800) << 10) + (static_cast<char32_t>(trail) - 0xDC00);
            }
        }
        return lead;
    }

    bool decodeBase64(const std::string_view src, std::vector<uint8_t>& dst)
    {
        static constexpr auto table = [] {
            std::array<int8_t, 256> t{};
            t.fill(-1);
            for (auto c = 'A'; c <= 'Z'; ++c)
            {
                t[static_cast<uint8_t>(c)] = static_cast<int8_t>(c - 'A');
            }
            for (auto c = 'a'; c <= 'z'; ++c)
            {
                t[static_cast<uint8_t>(c)] = static_cast<int8_t>(c - 'a' + 26);
            }
            for (auto c = '0'; c <= '9'; ++c)
            {
                t[static_cast<uint8_t>(c)] = static_cast<int8_t>(c - '0' + 52);
            }
            t['+'] = 62;
            t['/'] = 63;
            t['-'] = 62;
            t['_'] = 63;
            return t;
        }();

        dst.clear();
        dst.reserve(src.size() / 4 * 3 + 3);
        uint32_t accumulator = 0;
        int bits = 0;
        for (const auto ch : src)
        {
            if (ch == '=')
            {
                break;
            }
            const auto value = til::at(table, static_cast<uint8_t>(ch));
            if (value < 0)
            {
                return false;
            }
            accumulator = (accumulator << 6) | static_cast<uint32_t>(value);
            bits += 6;
            if (bits >= 8)
            {
                bits -= 8;
                dst.push_back(static_cast<uint8_t>(accumulator >> bits));
            }
        }
        return true;
    }

    // WIC is created per call: images arrive rarely and the factory is cheap.
    // The output thread may not have COM yet, in which case it gets the MTA.
    wil::com_ptr_nothrow<IWICImagingFactory> wicFactory() noexcept
    {
        wil::com_ptr_nothrow<IWICImagingFactory> factory;
        auto hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(factory.addressof()));
        if (hr == CO_E_NOTINITIALIZED)
        {
            if (SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED)))
            {
                hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(factory.addressof()));
            }
        }
        return SUCCEEDED(hr) ? factory : nullptr;
    }

    bool copyPixels(IWICBitmapSource* source, const UINT width, const UINT height, std::vector<RGBQUAD>& pixels) noexcept
    {
        try
        {
            pixels.resize(size_t{ width } * height);
        }
        catch (...)
        {
            return false;
        }
        const auto stride = width * 4;
        return SUCCEEDED(source->CopyPixels(nullptr, stride, stride * height, reinterpret_cast<BYTE*>(pixels.data())));
    }
}

KittyGraphics::KittyGraphics(AdaptDispatch& dispatcher) noexcept :
    _dispatcher{ dispatcher }
{
}

void KittyGraphics::Clear() noexcept
{
    _images.clear();
    _pending.reset();
    _totalBytes = 0;
}

void KittyGraphics::Command(const std::wstring_view command)
{
    const auto separator = command.find(L';');
    const auto control = command.substr(0, separator);
    const auto payload = separator == std::wstring_view::npos ? std::wstring_view{} : command.substr(separator + 1);

    // Keys are one letter; values are integers, or one character for the
    // keys that take a letter (a=T, t=d, d=i ...).
    std::map<wchar_t, int64_t> keys;
    for (size_t start = 0; start < control.size();)
    {
        const auto end = std::min(control.find(L',', start), control.size());
        const auto pair = control.substr(start, end - start);
        if (pair.size() >= 3 && pair[1] == L'=')
        {
            const auto value = pair.substr(2);
            if (value[0] == L'-' || (value[0] >= L'0' && value[0] <= L'9'))
            {
                int64_t number = 0;
                const auto negative = value[0] == L'-';
                for (auto ch : value.substr(negative ? 1 : 0))
                {
                    if (ch < L'0' || ch > L'9' || number > 0xFFFFFFFF)
                    {
                        break;
                    }
                    number = number * 10 + (ch - L'0');
                }
                keys[pair[0]] = negative ? -number : number;
            }
            else
            {
                keys[pair[0]] = value[0];
            }
        }
        start = end + 1;
    }

    std::string data;
    data.reserve(payload.size());
    for (const auto ch : payload)
    {
        data.push_back(ch < 0x80 ? static_cast<char>(ch) : '?');
    }

    const auto more = keys.contains(L'm') && keys[L'm'] == 1;
    const auto onlyChunkKeys = std::all_of(keys.begin(), keys.end(), [](const auto& kv) { return kv.first == L'm' || kv.first == L'q'; });

    if (_pending && onlyChunkKeys)
    {
        if (_pending->data.size() + data.size() > maxEncodedBytes)
        {
            _respond(*_pending, L"EFBIG:image data too large");
            _pending.reset();
            return;
        }
        _pending->data.append(data);
        if (keys.contains(L'q'))
        {
            _pending->keys[L'q'] = keys[L'q'];
        }
        if (!more)
        {
            auto t = std::move(*_pending);
            _pending.reset();
            _execute(t);
        }
        return;
    }

    // A new command while a transmission was open abandons that transmission.
    _pending.reset();
    Transmission t{ std::move(keys), std::move(data) };
    if (more)
    {
        _pending = std::move(t);
        return;
    }
    _execute(t);
}

void KittyGraphics::_execute(Transmission& t)
{
    const auto key = [&](wchar_t k, int64_t fallback) { return t.keys.contains(k) ? t.keys[k] : fallback; };
    const auto action = static_cast<wchar_t>(key(L'a', L't'));
    const auto id = static_cast<uint32_t>(key(L'i', 0));

    switch (action)
    {
    case L'q':
    {
        Image probe;
        std::wstring error;
        _respond(t, _decode(t, probe, error) ? L"OK" : error);
        return;
    }
    case L't':
    case L'T':
    {
        Image image;
        std::wstring error;
        if (!_decode(t, image, error))
        {
            _respond(t, error);
            return;
        }
        const auto bytes = image.pixels.size() * sizeof(RGBQUAD);
        if (id != 0)
        {
            if (const auto old = _images.find(id); old != _images.end())
            {
                _totalBytes -= old->second.pixels.size() * sizeof(RGBQUAD);
                _images.erase(old);
            }
        }
        if (_totalBytes + bytes > maxTotalBytes)
        {
            _respond(t, L"ENOSPC:image storage full");
            return;
        }
        if (action == L'T')
        {
            if (key(L'U', 0) == 1)
            {
                image.columns = static_cast<til::CoordType>(key(L'c', 0));
                image.rows = static_cast<til::CoordType>(key(L'r', 0));
            }
            else
            {
                _placeDirect(id, image, t);
            }
        }
        if (id != 0)
        {
            _totalBytes += bytes;
            _images.insert_or_assign(id, std::move(image));
        }
        _respond(t, L"OK");
        return;
    }
    case L'p':
    {
        const auto it = _images.find(id);
        if (it == _images.end())
        {
            _respond(t, L"ENOENT:no such image");
            return;
        }
        if (key(L'U', 0) == 1)
        {
            it->second.columns = static_cast<til::CoordType>(key(L'c', 0));
            it->second.rows = static_cast<til::CoordType>(key(L'r', 0));
            it->second.canvas.clear();
        }
        else
        {
            _placeDirect(id, it->second, t);
        }
        _respond(t, L"OK");
        return;
    }
    case L'd':
    {
        const auto what = static_cast<wchar_t>(key(L'd', L'a'));
        if (what == L'a' || what == L'A')
        {
            _images.clear();
            _totalBytes = 0;
        }
        else if ((what == L'i' || what == L'I') && id != 0)
        {
            if (const auto it = _images.find(id); it != _images.end())
            {
                _totalBytes -= it->second.pixels.size() * sizeof(RGBQUAD);
                _images.erase(it);
            }
        }
        return;
    }
    default:
        return;
    }
}

bool KittyGraphics::_decode(const Transmission& t, Image& image, std::wstring& error)
{
    const auto key = [&](wchar_t k, int64_t fallback) {
        const auto it = t.keys.find(k);
        return it != t.keys.end() ? it->second : fallback;
    };

    if (key(L't', L'd') != L'd')
    {
        error = L"EINVAL:only direct transmission (t=d) is supported";
        return false;
    }
    if (t.keys.contains(L'o'))
    {
        error = L"EINVAL:compression is not supported";
        return false;
    }

    std::vector<uint8_t> bytes;
    if (!decodeBase64(t.data, bytes))
    {
        error = L"EINVAL:bad base64 data";
        return false;
    }

    const auto format = key(L'f', 32);
    if (format == 100)
    {
        const auto factory = wicFactory();
        wil::com_ptr_nothrow<IWICStream> stream;
        wil::com_ptr_nothrow<IWICBitmapDecoder> decoder;
        wil::com_ptr_nothrow<IWICBitmapFrameDecode> frame;
        wil::com_ptr_nothrow<IWICFormatConverter> converter;
        UINT width = 0;
        UINT height = 0;
        if (!factory ||
            bytes.size() > UINT32_MAX ||
            FAILED(factory->CreateStream(stream.addressof())) ||
            FAILED(stream->InitializeFromMemory(bytes.data(), static_cast<DWORD>(bytes.size()))) ||
            FAILED(factory->CreateDecoderFromStream(stream.get(), nullptr, WICDecodeMetadataCacheOnDemand, decoder.addressof())) ||
            FAILED(decoder->GetFrame(0, frame.addressof())) ||
            FAILED(frame->GetSize(&width, &height)) ||
            width == 0 || height == 0 || width > maxImageSide || height > maxImageSide ||
            FAILED(factory->CreateFormatConverter(converter.addressof())) ||
            FAILED(converter->Initialize(frame.get(), GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeCustom)) ||
            !copyPixels(converter.get(), width, height, image.pixels))
        {
            error = L"EBADPNG:could not decode the PNG";
            return false;
        }
        image.width = static_cast<til::CoordType>(width);
        image.height = static_cast<til::CoordType>(height);
        return true;
    }

    if (format == 24 || format == 32)
    {
        const auto width = key(L's', 0);
        const auto height = key(L'v', 0);
        const auto depth = format == 32 ? 4 : 3;
        if (width <= 0 || height <= 0 || width > maxImageSide || height > maxImageSide)
        {
            error = L"EINVAL:raw data needs s and v";
            return false;
        }
        const auto count = static_cast<size_t>(width) * static_cast<size_t>(height);
        if (bytes.size() < count * depth)
        {
            error = L"ENODATA:not enough pixel data";
            return false;
        }
        image.width = static_cast<til::CoordType>(width);
        image.height = static_cast<til::CoordType>(height);
        image.pixels.resize(count);
        for (size_t i = 0; i < count; ++i)
        {
            const auto src = &bytes[i * depth];
            const auto alpha = depth == 4 ? src[3] : uint8_t{ 255 };
            // The renderer takes premultiplied alpha.
            const auto mul = [alpha](uint8_t c) { return static_cast<BYTE>((c * alpha + 127) / 255); };
            image.pixels[i] = RGBQUAD{ mul(src[2]), mul(src[1]), mul(src[0]), alpha };
        }
        return true;
    }

    error = L"EINVAL:unsupported format";
    return false;
}

const std::vector<RGBQUAD>& KittyGraphics::_canvas(Image& image)
{
    // Without a box, the image covers as many cells as its pixels need.
    const auto columns = image.columns > 0 ? image.columns : (image.width + CellSize.width - 1) / CellSize.width;
    const auto rows = image.rows > 0 ? image.rows : (image.height + CellSize.height - 1) / CellSize.height;
    if (!image.canvas.empty() && image.canvasCells == til::size{ columns, rows })
    {
        return image.canvas;
    }

    image.canvasCells = { columns, rows };
    const auto canvasWidth = columns * CellSize.width;
    const auto canvasHeight = rows * CellSize.height;
    image.canvas.assign(static_cast<size_t>(canvasWidth) * static_cast<size_t>(canvasHeight), RGBQUAD{});

    // Fit the image into the box, keeping its aspect ratio, centred.
    const auto scale = std::min(static_cast<double>(canvasWidth) / image.width, static_cast<double>(canvasHeight) / image.height);
    const auto fitWidth = std::max(1, static_cast<til::CoordType>(image.width * scale));
    const auto fitHeight = std::max(1, static_cast<til::CoordType>(image.height * scale));
    const auto offsetX = (canvasWidth - fitWidth) / 2;
    const auto offsetY = (canvasHeight - fitHeight) / 2;

    std::vector<RGBQUAD> fitted;
    const auto factory = wicFactory();
    wil::com_ptr_nothrow<IWICBitmap> bitmap;
    wil::com_ptr_nothrow<IWICBitmapScaler> scaler;
    if (!factory ||
        FAILED(factory->CreateBitmapFromMemory(image.width, image.height, GUID_WICPixelFormat32bppPBGRA, image.width * 4, gsl::narrow_cast<UINT>(image.pixels.size() * 4), reinterpret_cast<BYTE*>(image.pixels.data()), bitmap.addressof())) ||
        FAILED(factory->CreateBitmapScaler(scaler.addressof())) ||
        FAILED(scaler->Initialize(bitmap.get(), fitWidth, fitHeight, WICBitmapInterpolationModeHighQualityCubic)) ||
        !copyPixels(scaler.get(), fitWidth, fitHeight, fitted))
    {
        return image.canvas;
    }

    for (til::CoordType y = 0; y < fitHeight; ++y)
    {
        const auto src = fitted.begin() + static_cast<ptrdiff_t>(y) * fitWidth;
        const auto dst = image.canvas.begin() + static_cast<ptrdiff_t>(y + offsetY) * canvasWidth + offsetX;
        std::copy_n(src, fitWidth, dst);
    }
    return image.canvas;
}

void KittyGraphics::_paintCell(Page& page, const til::point at, Image& image, const til::CoordType row, const til::CoordType column)
{
    const auto& canvas = _canvas(image);
    const auto [columns, rows] = image.canvasCells;
    if (canvas.empty() || row < 0 || column < 0 || row >= rows || column >= columns)
    {
        return;
    }

    auto& buffer = page.Buffer();
    auto& dstRow = buffer.GetMutableRowByOffset(at.y);
    auto slice = dstRow.GetMutableImageSlice();
    if (!slice || slice->CellSize() != CellSize)
    {
        slice = dstRow.SetImageSlice(std::make_unique<ImageSlice>(CellSize));
    }

    auto dst = slice->MutablePixels(at.x, at.x + 1);
    const auto canvasWidth = columns * CellSize.width;
    auto src = canvas.begin() + static_cast<ptrdiff_t>(row) * CellSize.height * canvasWidth + static_cast<ptrdiff_t>(column) * CellSize.width;
    for (auto y = 0; y < CellSize.height; ++y)
    {
        std::copy_n(src, CellSize.width, dst);
        src += canvasWidth;
        dst += slice->PixelWidth();
    }
    slice->BumpRevision();
}

void KittyGraphics::_placeDirect(const uint32_t /*id*/, Image& image, const Transmission& t)
{
    const auto key = [&](wchar_t k, int64_t fallback) {
        const auto it = t.keys.find(k);
        return it != t.keys.end() ? it->second : fallback;
    };
    image.columns = static_cast<til::CoordType>(key(L'c', 0));
    image.rows = static_cast<til::CoordType>(key(L'r', 0));
    image.canvas.clear();
    _canvas(image);

    auto page = _dispatcher._pages.ActivePage();
    auto& cursor = page.Cursor();
    const auto origin = cursor.GetPosition();
    const auto [columns, rows] = image.canvasCells;
    const auto right = std::min(origin.x + columns, page.Width());
    const auto bottom = std::min(origin.y + rows, page.Bottom());
    for (auto y = origin.y; y < bottom; ++y)
    {
        for (auto x = origin.x; x < right; ++x)
        {
            _paintCell(page, { x, y }, image, y - origin.y, x - origin.x);
        }
    }
    page.Buffer().TriggerRedraw(Microsoft::Console::Types::Viewport::FromExclusive({ origin.x, origin.y, right, bottom }));

    // C=1 leaves the cursor where it was; otherwise it moves past the image.
    if (key(L'C', 0) != 1)
    {
        cursor.SetPosition({ std::min(right, page.Width() - 1), std::max(origin.y, bottom - 1) });
    }
    // A direct placement does not keep its own copy of the canvas around.
    image.canvas.clear();
    image.columns = 0;
    image.rows = 0;
}

void KittyGraphics::_respond(const Transmission& t, const std::wstring_view message)
{
    const auto quiet = t.keys.contains(L'q') ? t.keys.at(L'q') : 0;
    const auto ok = message == L"OK";
    if (quiet >= 2 || (quiet == 1 && ok))
    {
        return;
    }
    // Kitty answers only commands that name an image.
    const auto id = t.keys.contains(L'i') ? t.keys.at(L'i') : 0;
    const auto number = t.keys.contains(L'I') ? t.keys.at(L'I') : 0;
    if (id == 0 && number == 0)
    {
        return;
    }
    std::wstring response{ L"\x1b_G" };
    if (id != 0)
    {
        fmt::format_to(std::back_inserter(response), FMT_COMPILE(L"i={}"), id);
    }
    if (number != 0)
    {
        fmt::format_to(std::back_inserter(response), FMT_COMPILE(L"{}I={}"), id != 0 ? L"," : L"", number);
    }
    response += L';';
    response += message;
    response += L"\x1b\\";
    _dispatcher._api.ReturnResponse(response);
}

til::CoordType KittyGraphics::_diacriticIndex(const char32_t ch) noexcept
{
    const auto it = std::lower_bound(diacritics.begin(), diacritics.end(), ch);
    return it != diacritics.end() && *it == ch ? static_cast<til::CoordType>(it - diacritics.begin()) : -1;
}

uint32_t KittyGraphics::_currentImageId() const noexcept
{
    const auto foreground = _dispatcher._pages.ActivePage().Attributes().GetForeground();
    if (foreground.IsRgb())
    {
        return (uint32_t{ foreground.GetR() } << 16) | (uint32_t{ foreground.GetG() } << 8) | foreground.GetB();
    }
    if (foreground.IsIndex256() || foreground.IsIndex16())
    {
        return foreground.GetIndex();
    }
    return 0;
}

void KittyGraphics::WriteWithPlaceholders(const std::wstring_view text, const std::function<void(std::wstring_view)>& writeText)
{
    size_t segmentStart = 0;
    size_t i = 0;
    while (i < text.size())
    {
        const auto start = i;
        if (readCodePoint(text, i) != Placeholder)
        {
            continue;
        }
        if (start > segmentStart)
        {
            writeText(text.substr(segmentStart, start - segmentStart));
        }

        // Up to three diacritics follow: row, column, and the image id's
        // highest byte.
        std::array<til::CoordType, 3> marks{ -1, -1, -1 };
        for (auto& mark : marks)
        {
            auto peek = i;
            if (peek >= text.size())
            {
                break;
            }
            const auto index = _diacriticIndex(readCodePoint(text, peek));
            if (index < 0)
            {
                break;
            }
            mark = index;
            i = peek;
        }
        segmentStart = i;

        auto id = _currentImageId();
        if (marks[2] >= 0)
        {
            id |= static_cast<uint32_t>(marks[2]) << 24;
        }

        // Missing marks continue the run in the cell to the left.
        auto page = _dispatcher._pages.ActivePage();
        const auto before = page.Cursor().GetPosition();
        const auto continues = _lastCell.imageId == id && _lastCellAt == til::point{ before.x - 1, before.y };
        Cell cell{ id, marks[0], marks[1] };
        if (cell.row < 0)
        {
            cell.row = continues ? _lastCell.row : 0;
        }
        if (cell.column < 0)
        {
            cell.column = continues && cell.row == _lastCell.row ? _lastCell.column + 1 : 0;
        }

        writeText(L" ");

        // The space lands where the cursor was, unless a pending wrap moved
        // it to the next line first. A cursor still holding a delayed wrap is
        // sitting on the cell it just wrote.
        const auto& cursor = page.Cursor();
        const auto after = cursor.GetPosition();
        const auto written = cursor.GetDelayEOLWrap() ? after : til::point{ after.x - 1, after.y };
        if (const auto it = _images.find(id); it != _images.end() && written.x >= 0)
        {
            _paintCell(page, written, it->second, cell.row, cell.column);
            page.Buffer().TriggerRedraw(Microsoft::Console::Types::Viewport::FromExclusive({ written.x, written.y, written.x + 1, written.y + 1 }));
        }
        _lastCell = cell;
        _lastCellAt = written;
    }
    if (segmentStart < text.size())
    {
        writeText(text.substr(segmentStart));
    }
}
