#ifndef CWOFFSCREENATLASGRID_H
#define CWOFFSCREENATLASGRID_H

//Qt includes
#include <QRect>
#include <QSize>
#include <QtTypes>

//Std includes
#include <algorithm>

/**
 * @brief Regular grid layout for packing uniform offscreen tiles into one atlas texture.
 *
 * Pure geometry, no GPU resources. The offscreen renderer's atlas-batching path
 * (see plans/OFFSCREEN_ATLAS_BATCHING_PLAN) renders each tile into a reused scratch and
 * copies it into the atlas sub-rect this grid hands out, then reads the whole atlas back
 * once. Kept separate from cwRhiOffscreenRenderer so the sizing/mapping math can be
 * unit-tested without a live QRhi.
 *
 * choose() picks the grid from three independent caps, smallest wins:
 *   - the backend's max texture dimension (the atlas never exceeds it on either axis),
 *   - a byte budget covering the atlas (RGBA8, colour-only) plus the one scratch the
 *     renderer reuses (RGBA8 colour + depth), and
 *   - a per-frame GPU work cap (max tiles drawn + copied into one command buffer).
 * A batch larger than capacity() spills to the next frame.
 */
struct cwOffscreenAtlasGrid {
    QSize tileSize;    // per-tile size in pixels (every tile in a batch is identical)
    int columns = 0;   // tiles across the atlas
    int rows = 0;      // tiles down the atlas

    bool isValid() const { return columns > 0 && rows > 0 && !tileSize.isEmpty(); }

    // Cells the atlas holds — the most tiles one frame can process.
    int capacity() const { return columns * rows; }

    // Full atlas texture size: columns*tileW by rows*tileH.
    QSize atlasSize() const
    {
        return { columns * tileSize.width(), rows * tileSize.height() };
    }

    // Atlas size needed to hold exactly @a count tiles using this grid's column count:
    // full width, but only ceil(count/columns) rows tall. tileRect() lays tiles out
    // row-major by columns alone, so a shorter atlas holds the same cells — letting a
    // partial batch allocate and read back only the rows it fills instead of atlasSize().
    // Clamped to [1, rows]; equals atlasSize() once count reaches capacity().
    // An invalid grid (columns or rows <= 0) returns atlasSize() (an empty QSize),
    // guarding the ceil-divide against /0 and std::clamp against a low > high range.
    QSize atlasSizeForCount(int count) const
    {
        if (columns <= 0 || rows <= 0) {
            return atlasSize();
        }
        const int neededRows = (count + columns - 1) / columns;
        const int usedRows = std::clamp(neededRows, 1, rows);
        return { columns * tileSize.width(), usedRows * tileSize.height() };
    }

    // Sub-rectangle (in atlas pixels) for tile @a index, laid out row-major from the
    // top-left. Returns a null QRect for an out-of-range index.
    QRect tileRect(int index) const;

    // Choose a grid for @a tileSize honouring all three caps (see the struct doc).
    // @a depthBytesPerPixel is the scratch depth texture's per-pixel cost (e.g. 4 for
    // D24S8 / D32F). Returns an invalid grid (0x0) when not even one tile fits every cap;
    // the caller then uses the one-tile-per-frame fallback. capacity() is bounded by the
    // smallest cap but, because the layout is kept square-ish, may sit a few cells below
    // it for awkward cap values — never above.
    static cwOffscreenAtlasGrid choose(QSize tileSize, int textureSizeMax,
                                       qint64 byteBudget, int maxTilesPerFrame,
                                       int depthBytesPerPixel = 4);
};

#endif // CWOFFSCREENATLASGRID_H
