#ifndef CWKTX2CODEC_H
#define CWKTX2CODEC_H

// Qt includes
#include <QByteArray>
#include <QImage>
#include <QSize>
#include <QVector>

// Qt RHI
#include <rhi/qrhi.h>

// Our includes
#include "CaveWhereLibExport.h"
#include "cwDiskCacher.h"

// Monad includes
#include "Monad/Result.h"

/**
 * A transcoded, GPU-ready texture. Mip levels are ordered level 0 first and
 * each entry holds tightly packed block data for that level.
 */
struct CAVEWHERE_LIB_EXPORT cwCompressedTexture
{
    QRhiTexture::Format format = QRhiTexture::UnknownFormat;
    QSize size;
    QVector<QByteArray> mipLevels;

    bool isNull() const
    {
        return format == QRhiTexture::UnknownFormat || size.isEmpty() || mipLevels.isEmpty();
    }
};

/**
 * Stateless KTX2 / Basis UASTC helpers. Every function is pure and safe to call
 * from cwConcurrent worker threads.
 */
namespace cw::ktx2 {

    /**
     * The default UASTC encoder quality level, matching libktx's
     * KTX_PACK_UASTC_LEVEL_DEFAULT.
     */
    constexpr int kDefaultUastcQuality = 2;

    /**
     * Encodes image as a UASTC supercompressed .ktx2 file with a full mip chain
     * (built CPU-side, halving down to 1x1) and returns the serialized bytes.
     */
    CAVEWHERE_LIB_EXPORT Monad::Result<QByteArray> encodeRgba(const QImage& image,
                                                              int quality = kDefaultUastcQuality);

    /**
     * Transcodes .ktx2 bytes produced by encodeRgba() into target, which must be
     * QRhiTexture::BC7 or QRhiTexture::ASTC_4x4.
     */
    CAVEWHERE_LIB_EXPORT Monad::Result<cwCompressedTexture> transcode(const QByteArray& ktx2Bytes,
                                                                      QRhiTexture::Format target);

    /**
     * Returns BC7 when rhi supports it, otherwise ASTC_4x4, otherwise
     * UnknownFormat so callers fall back to the uncompressed RGBA8 path.
     *
     * On Apple silicon Metal both BC7 and ASTC_4x4 are typically available; BC7
     * is preferred there so desktop platforms all take the same path.
     */
    CAVEWHERE_LIB_EXPORT QRhiTexture::Format preferredCompressedFormat(QRhi* rhi);

    /**
     * The compressed format this build targets, for callers that must choose one
     * without a QRhi in hand — the GUI thread has no QRhi, so scrap and glTF
     * textures are transcoded before they ever reach the render thread. Desktop
     * builds target BC7, mobile builds (iOS, Android) ASTC_4x4. Pair it with
     * supportedCompressedFormat(): compress only while the two agree, because a
     * compressed texture the backend rejects leaves the item textureless.
     */
    CAVEWHERE_LIB_EXPORT QRhiTexture::Format targetCompressedFormat();

    /**
     * What the render backend actually accepts, published by the render thread
     * once it has a QRhi and readable from any thread. UnknownFormat until the
     * first frame, so callers stay on the uncompressed path while the device's
     * capabilities are still unknown.
     */
    CAVEWHERE_LIB_EXPORT QRhiTexture::Format supportedCompressedFormat();
    CAVEWHERE_LIB_EXPORT void setSupportedCompressedFormat(QRhiTexture::Format format);

    /**
     * Reads the .ktx2 bytes stored at key and transcodes them to target.
     * Returns an error Result when the entry is missing or the bytes fail to
     * transcode, so callers can fall back to the uncompressed image.
     */
    CAVEWHERE_LIB_EXPORT Monad::Result<cwCompressedTexture> transcodeFromCache(const cwDiskCacher& cacher,
                                                                              const cwDiskCacher::Key& key,
                                                                              QRhiTexture::Format target);
}

#endif // CWKTX2CODEC_H
