#ifndef CWSTREAMEDTEXTURE_H
#define CWSTREAMEDTEXTURE_H

// Qt includes
#include <QDir>
#include <QSize>
#include <QString>

// Our includes
#include "CaveWhereLibExport.h"
#include "cwDiskCacher.h"

/**
 * What a texture producer publishes instead of pixels: where the KTX2 bytes
 * live and how large level 0 is. A plain copyable value — reading the bytes is
 * cw::ktx2::loadStreamedLevels()'s job, so nothing here touches disk.
 *
 * The data root is kept cleaned and absolute so every spelling of one directory
 * compares equal. operator== drives the re-publish no-op in
 * cwRenderTexturedItems and the dedup in cwTextureStreamer, so two spellings of
 * one root would otherwise restart residency from the pinned base.
 */
struct CAVEWHERE_LIB_EXPORT cwStreamedTexture
{
    cwStreamedTexture() = default;

    cwStreamedTexture(const QString& dataRootPath,
                      const cwDiskCacher::Key& key,
                      const QSize& size) :
        key(key),
        size(size),
        m_dataRootPath(normalized(dataRootPath))
    {
    }

    cwDiskCacher::Key key;     //The KTX2 entry: UASTC with a full mip chain
    QSize size;                //Level-0 dimensions, known without decoding

    //Project data root, the cwDiskCacher root, cleaned and absolute
    const QString& dataRootPath() const
    {
        return m_dataRootPath;
    }

    void setDataRootPath(const QString& dataRootPath)
    {
        m_dataRootPath = normalized(dataRootPath);
    }

    bool isNull() const
    {
        return m_dataRootPath.isEmpty() || key.id.isEmpty() || size.isEmpty();
    }

    bool operator==(const cwStreamedTexture& other) const
    {
        return m_dataRootPath == other.m_dataRootPath
               && key.id == other.key.id
               && key.path == other.key.path
               && key.checksum == other.key.checksum
               && size == other.size;
    }

private:
    static QString normalized(const QString& dataRootPath)
    {
        //QDir("").absolutePath() answers the working directory, so an empty
        //root stays empty and keeps isNull() true.
        return dataRootPath.isEmpty() ? QString() : QDir(dataRootPath).absolutePath();
    }

    QString m_dataRootPath;
};

#endif // CWSTREAMEDTEXTURE_H
