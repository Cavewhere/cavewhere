#ifndef CWMIPMATH_H
#define CWMIPMATH_H

// Qt includes
#include <QSize>

// Qt RHI
#include <rhi/qrhi.h>

// Std includes
#include <algorithm>

/**
 * The one home for mip-chain shape and size math. Encode, residency selection,
 * and the memory ledger all answer these questions the same way. Every function
 * is pure, header-only, and safe to call from any thread.
 */
namespace cw::mip {

    namespace detail {
        constexpr int kSmallestMipDimension = 1;
        constexpr int kMipDivisor = 2;
        constexpr int kBytesPerCompressedBlock = 16;
        constexpr int kCompressedBlockDimension = 4;
        constexpr int kBytesPerRgbaPixel = 4;

        constexpr int blockDimensionFor(QRhiTexture::Format format)
        {
            switch(format) {
            case QRhiTexture::BC7:
            case QRhiTexture::ASTC_4x4:
                return kCompressedBlockDimension;
            default:
                return 0;
            }
        }

        constexpr qint64 blocksAcross(int pixels, int blockDimension)
        {
            return (qint64(pixels) + blockDimension - 1) / blockDimension;
        }

        constexpr QSize halved(QSize size)
        {
            return QSize(std::max(kSmallestMipDimension, size.width() / kMipDivisor),
                         std::max(kSmallestMipDimension, size.height() / kMipDivisor));
        }
    }

    /**
     * The number of mip levels in the chain for level0, halving each axis down
     * to 1x1. Matches the chain cw::ktx2::encodeRgba() writes. An empty size
     * has no levels.
     */
    inline int mipLevelCount(QSize level0)
    {
        if(level0.width() <= 0 || level0.height() <= 0) {
            return 0;
        }

        int levels = 1;
        QSize size = level0;
        while(size.width() > detail::kSmallestMipDimension
              || size.height() > detail::kSmallestMipDimension) {
            size = detail::halved(size);
            levels++;
        }
        return levels;
    }

    /**
     * The dimensions of level in a chain that starts at level0: successive
     * halving with each axis floored at 1.
     */
    inline QSize mipLevelSize(QSize level0, int level)
    {
        if(level0.width() <= 0 || level0.height() <= 0) {
            return QSize();
        }

        QSize size = level0;
        for(int i = 0; i < level; i++) {
            size = detail::halved(size);
        }
        return size;
    }

    /**
     * The exact byte count of one mip level of the given format. Block formats
     * round the level dimensions up to whole blocks. Formats other than BC7,
     * ASTC_4x4, and RGBA8 return 0.
     */
    inline qint64 mipLevelBytes(QRhiTexture::Format format, QSize levelSize)
    {
        if(levelSize.width() <= 0 || levelSize.height() <= 0) {
            return 0;
        }

        const int blockDimension = detail::blockDimensionFor(format);
        if(blockDimension > 0) {
            return detail::blocksAcross(levelSize.width(), blockDimension)
                   * detail::blocksAcross(levelSize.height(), blockDimension)
                   * detail::kBytesPerCompressedBlock;
        }

        if(format == QRhiTexture::RGBA8) {
            return qint64(levelSize.width()) * qint64(levelSize.height())
                   * detail::kBytesPerRgbaPixel;
        }

        return 0;
    }

    /**
     * The bytes held by levels topLevel through the 1x1 tail — the cost of a
     * texture whose most detailed resident level is topLevel.
     */
    inline qint64 chainBytes(QRhiTexture::Format format, QSize level0, int topLevel)
    {
        const int levels = mipLevelCount(level0);
        qint64 bytes = 0;
        for(int level = std::max(0, topLevel); level < levels; level++) {
            bytes += mipLevelBytes(format, mipLevelSize(level0, level));
        }
        return bytes;
    }
}

#endif // CWMIPMATH_H
