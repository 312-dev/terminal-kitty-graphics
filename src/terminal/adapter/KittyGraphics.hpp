/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- KittyGraphics.hpp

Abstract:
- A subset of the kitty graphics protocol: enough for a client to transmit
  an image, query support, and show it with Unicode placeholders (U+10EEEE)
  or a direct placement at the cursor. Animation, relative placement, file
  and shared-memory transmission and compression are not supported.
--*/

#pragma once

#include "til.h"

namespace Microsoft::Console::VirtualTerminal
{
    class AdaptDispatch;
    class Page;

    class KittyGraphics
    {
    public:
        // Placeholder code point, and its UTF-16 surrogate pair.
        static constexpr char32_t Placeholder = 0x10EEEE;
        static constexpr wchar_t PlaceholderLead = 0xDBFB;
        static constexpr wchar_t PlaceholderTrail = 0xDEEE;

        // Each cell of a kitty image is stored in an ImageSlice CellHeight
        // pixels tall and as wide as the font's own cell shape gives (see
        // _cellSize); the renderer scales it to the real cell, so a picture
        // keeps its aspect ratio whatever the font.
        static constexpr til::CoordType CellHeight = 40;

        explicit KittyGraphics(AdaptDispatch& dispatcher) noexcept;

        // One APC payload, everything after the leading 'G'.
        void Command(std::wstring_view command);
        // Writes text that contains placeholders: the text around them goes
        // through writeText, each placeholder becomes a painted space.
        void WriteWithPlaceholders(std::wstring_view text, const std::function<void(std::wstring_view)>& writeText);
        // True while the last placeholder written may still get marks from
        // the next text (see OpenCell): that text must come here even when
        // it holds no placeholder.
        bool HasOpenCell() const noexcept;
        void Clear() noexcept;

    private:
        struct Image
        {
            til::CoordType width = 0;
            til::CoordType height = 0;
            std::vector<RGBQUAD> pixels;
            // Virtual placement box, in cells. 0 means derive from the size.
            til::CoordType columns = 0;
            til::CoordType rows = 0;
            // The image fitted into columns x rows cells of canvasCellSize,
            // built on first paint and again when the font's shape changes.
            til::size canvasCells;
            til::size canvasCellSize;
            std::vector<RGBQUAD> canvas;
        };

        struct Transmission
        {
            std::map<wchar_t, int64_t> keys;
            std::string data;
        };

        struct Cell
        {
            uint32_t imageId = 0;
            til::CoordType row = -1;
            til::CoordType column = -1;
        };

        using Marks = std::array<til::CoordType, 3>;

        // A placeholder that ended its text with fewer than three marks.
        // ConPTY passes output on in pieces cut anywhere, so the rest of its
        // marks can start the next text; they are read there and the cell is
        // painted again, instead of being drawn as combining characters.
        struct OpenCell
        {
            uint32_t baseId = 0;
            Marks marks{ -1, -1, -1 };
            size_t count = 0;
            // The cell to its left when it was written, for missing marks.
            Cell previous;
            bool adjacent = false;
            til::point written;
            til::point cursorAfter;
        };

        void _execute(Transmission& t);
        bool _decode(const Transmission& t, Image& image, std::wstring& error);
        void _placeDirect(uint32_t id, Image& image, const Transmission& t);
        til::size _cellSize() const noexcept;
        const std::vector<RGBQUAD>& _canvas(Image& image);
        void _paintCell(Page& page, til::point at, Image& image, til::CoordType row, til::CoordType column);
        void _respond(const Transmission& t, std::wstring_view message);
        static til::CoordType _diacriticIndex(char32_t ch) noexcept;
        static size_t _readMarks(std::wstring_view text, size_t& i, Marks& marks, size_t count) noexcept;
        static Cell _cellFor(uint32_t baseId, const Marks& marks, const Cell& previous, bool adjacent) noexcept;
        void _paintPlaced(Page& page, til::point at, const Cell& cell);
        uint32_t _currentImageId() const noexcept;

        AdaptDispatch& _dispatcher;
        std::unordered_map<uint32_t, Image> _images;
        std::optional<Transmission> _pending;
        Cell _lastCell;
        til::point _lastCellAt{ -1, -1 };
        std::optional<OpenCell> _open;
        size_t _totalBytes = 0;
    };
}
