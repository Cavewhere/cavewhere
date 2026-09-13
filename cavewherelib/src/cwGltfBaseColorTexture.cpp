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
    QString fileChecksum(const QString& filename, const cwProgressScope& parent)
    {
        QFile file(filename);
        if(!file.open(QIODevice::ReadOnly)) {
            return {};
        }

        cwProgressScope checksum(parent, QStringLiteral("Checksum"));
        checksum.setTotal(file.size());

        XXH3_state_t* state = XXH3_createState();
        if(state == nullptr) {
            return {};
        }

        XXH3_64bits_reset(state);
        qint64 bytesRead = 0;
        while(!file.atEnd()) {
            const QByteArray chunk = file.read(kChecksumChunkBytes);
            if(chunk.isEmpty()) {
                break;
            }
            XXH3_64bits_update(state, chunk.constData(), static_cast<size_t>(chunk.size()));
            bytesRead += chunk.size();
            checksum.report(bytesRead);
        }

        const XXH64_hash_t hash = XXH3_64bits_digest(state);
        XXH3_freeState(state);

        return QString::number(static_cast<quint64>(hash), kHashRadix);
    }
}

cwGltfBaseColorTexture::cwGltfBaseColorTexture(const QString& dataRootPath,
                                               const QString& gltfFilename,
                                               const cwProgressScope& parent) :
    m_dataRootPath(dataRootPath),
    m_gltfFilename(gltfFilename),
    m_fileChecksum(dataRootPath.isEmpty() ? QString() : fileChecksum(gltfFilename, parent))
{
}

void cwGltfBaseColorTexture::setOn(cwRenderTexturedItems::Item& item,
                                   const cw::gltf::SceneCPU& scene,
                                   const cw::gltf::MaterialCPU& material,
                                   const cwProgressScope& parent) const
{
    const cwStreamedTexture streamed = streamedSource(scene, material.baseColorTextureIndex, parent);

    //Only one of the two travels to the render thread, and the descriptor path
    //never decodes: baseColorImage() runs only when there is no entry to stream.
    item.streamedTexture = streamed;
    item.texture = streamed.isNull() ? cw::gltf::baseColorImage(scene, material) : QImage();
}

cwStreamedTexture cwGltfBaseColorTexture::streamedSource(const cw::gltf::SceneCPU& scene,
                                                         int textureIndex,
                                                         const cwProgressScope& parent) const
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
            gltfInfo.fileName()
                + QStringLiteral("-texture")
                + QString::number(textureIndex)
                + QStringLiteral("-uastc.ktx2"),
            gltfInfo.dir(),
            m_fileChecksum
        },
        size
    };

    cwDiskCacher cacher{QDir(m_dataRootPath)};
    //entry() rather than hasEntry(): the file name leaves the checksum out, so
    //reading the entry is what tells a current encode from a stale one.
    if(!cacher.entry(streamed.key).isEmpty()) {
        m_sourceByIndex.insert(textureIndex, streamed);
        return streamed;
    }

    //Only a miss has anything to compress, so a hit grows no node at all. The
    //encode can't say how far along it is, so the node stays opaque.
    const cwProgressScope compressing(parent, QStringLiteral("Compressing texture"));

    const auto encoded = cw::ktx2::encodeRgba(texture.toImage());
    if(encoded.hasError()) {
        qWarning() << "Can't encode the glTF texture, using the uncompressed image:"
                   << encoded.errorMessage();
        m_sourceByIndex.insert(textureIndex, cwStreamedTexture());
        return {};
    }

    cacher.insert(streamed.key, encoded.value());

    //insert() reports write failures (full or unwritable .cw_cache) by doing
    //nothing, and a descriptor pointing at a missing entry leaves the render
    //thread retrying a load that can never succeed. Read the entry back so a
    //failed write falls back to the uncompressed image instead.
    if(cacher.entry(streamed.key).isEmpty()) {
        qWarning() << "Can't cache the encoded glTF texture, using the uncompressed image:"
                   << streamed.key.id;
        m_sourceByIndex.insert(textureIndex, cwStreamedTexture());
        return {};
    }

    m_sourceByIndex.insert(textureIndex, streamed);
    return streamed;
}
