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
    const cwStreamedTexture streamed = streamedSource(scene, material.baseColorTextureIndex);

    //The descriptor path never decodes: baseColorImage() runs only when there
    //is no entry to stream.
    item.texture = streamed.isNull() ? cwItemTexture(cw::gltf::baseColorImage(scene, material))
                                     : cwItemTexture(streamed);
}

cwStreamedTexture cwGltfBaseColorTexture::streamedSource(const cw::gltf::SceneCPU& scene,
                                                         int textureIndex) const
{
    //Meshes of one scan share baseColor textures, so the first geometry's
    //answer serves every later one in this run instead of re-reading the cache
    //entry per item.
    const auto memoized = m_sourceByIndex.constFind(textureIndex);
    if(memoized != m_sourceByIndex.constEnd()) {
        return memoized.value();
    }

    if(m_fileChecksum.isEmpty()
        || textureIndex < 0
        || textureIndex >= scene.textures.size()) {
        return {};
    }

    const cw::gltf::TextureCPU& texture = scene.textures.at(textureIndex);
    const QSize size(texture.width, texture.height);
    if(size.isEmpty()) {
        m_sourceByIndex.insert(textureIndex, cwStreamedTexture());
        return {};
    }

    const QFileInfo gltfInfo(m_gltfFilename);
    const cwStreamedTexture streamed {
        m_dataRootPath,
        cwDiskCacher::Key {
            cw::ktx2::cacheKeyId(gltfInfo.fileName()
                                 + QStringLiteral("-texture")
                                 + QString::number(textureIndex))
                + QStringLiteral(".ktx2"),
            gltfInfo.dir(),
            m_fileChecksum
        },
        size
    };

    cwDiskCacher cacher{QDir(m_dataRootPath)};
    const auto ensured = cw::ktx2::ensureEncodedEntry(cacher, streamed.key, [&texture]() {
        return texture.toImage();
    });

    if(ensured.hasError()) {
        m_sourceByIndex.insert(textureIndex, cwStreamedTexture());
        return {};
    }

    m_sourceByIndex.insert(textureIndex, streamed);
    return streamed;
}
