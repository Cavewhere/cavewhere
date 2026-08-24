// Our includes
#include "cwGltfBaseColorTexture.h"
#include "cwDiskCacher.h"
#include "cwKtx2Codec.h"

// Qt includes
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>

// xxhash includes
#include "xxhash.h"

namespace {

    constexpr qint64 kChecksumChunkBytes = 1024 * 1024;
    constexpr int kHashRadix = 16;

    /**
     * The hash of the file's bytes, so a .glb that moved keeps its cache entries
     * and one that changed loses them. Empty when the file can't be read, which
     * leaves the caller on the uncompressed image.
     */
    QString fileChecksum(const QString& filename)
    {
        QFile file(filename);
        if(!file.open(QIODevice::ReadOnly)) {
            return {};
        }

        XXH3_state_t* state = XXH3_createState();
        if(state == nullptr) {
            return {};
        }

        XXH3_64bits_reset(state);
        while(!file.atEnd()) {
            const QByteArray chunk = file.read(kChecksumChunkBytes);
            if(chunk.isEmpty()) {
                break;
            }
            XXH3_64bits_update(state, chunk.constData(), static_cast<size_t>(chunk.size()));
        }

        const XXH64_hash_t hash = XXH3_64bits_digest(state);
        XXH3_freeState(state);

        return QString::number(static_cast<quint64>(hash), kHashRadix);
    }
}

cwGltfBaseColorTexture::cwGltfBaseColorTexture(const QString& dataRootPath,
                                               const QString& gltfFilename) :
    m_dataRootPath(dataRootPath),
    m_gltfFilename(gltfFilename),
    m_fileChecksum(dataRootPath.isEmpty() ? QString() : fileChecksum(gltfFilename))
{
}

void cwGltfBaseColorTexture::setOn(cwRenderTexturedItems::Item& item,
                                   const cw::gltf::SceneCPU& scene,
                                   const cw::gltf::MaterialCPU& material) const
{
    const QImage image = cw::gltf::baseColorImage(scene, material);
    const cwCompressedTexture compressed = compressedTexture(image, material.baseColorTextureIndex);

    //Only one of the two travels to the render thread, so dropping the decoded
    //RGBA frees it a full item build early.
    item.compressedTexture = compressed;
    item.texture = compressed.isNull() ? image : QImage();
}

cwCompressedTexture cwGltfBaseColorTexture::compressedTexture(const QImage& image, int textureIndex) const
{
    //Meshes of one scan share baseColor textures, so the first geometry's
    //transcode answers every later one in this run instead of re-reading and
    //re-transcoding the .ktx2 per item.
    const auto memoized = m_transcodedByIndex.constFind(textureIndex);
    if(memoized != m_transcodedByIndex.constEnd()) {
        return memoized.value();
    }

    //An item handed a texture the render backend rejects would have no texture
    //at all, so the compressed path waits until the first frame publishes what
    //the device accepts.
    const QRhiTexture::Format target = cw::ktx2::targetCompressedFormat();
    if(image.isNull()
        || m_fileChecksum.isEmpty()
        || cw::ktx2::supportedCompressedFormat() != target) {
        return {};
    }

    const QFileInfo gltfInfo(m_gltfFilename);
    const cwDiskCacher::Key key {
        gltfInfo.fileName()
            + QStringLiteral("-texture")
            + QString::number(textureIndex)
            + QStringLiteral("-uastc.ktx2"),
        gltfInfo.dir(),
        m_fileChecksum
    };

    cwDiskCacher cacher{QDir(m_dataRootPath)};
    const auto compressed = cw::ktx2::cachedCompressedTexture(cacher, key, image, target);
    if(compressed.hasError()) {
        qWarning() << "Can't compress the glTF texture, using the uncompressed image:"
                   << compressed.errorMessage();
        m_transcodedByIndex.insert(textureIndex, cwCompressedTexture());
        return {};
    }

    m_transcodedByIndex.insert(textureIndex, compressed.value());
    return compressed.value();
}
