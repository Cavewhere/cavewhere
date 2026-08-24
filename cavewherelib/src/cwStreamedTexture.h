#ifndef CWSTREAMEDTEXTURE_H
#define CWSTREAMEDTEXTURE_H

// Qt includes
#include <QSize>
#include <QString>

// Our includes
#include "CaveWhereLibExport.h"
#include "cwDiskCacher.h"

/**
 * What a texture producer publishes instead of pixels: where the KTX2 bytes
 * live and how large level 0 is. A plain copyable value — reading the bytes is
 * cw::ktx2::loadStreamedLevels()'s job, so nothing here touches disk.
 */
struct CAVEWHERE_LIB_EXPORT cwStreamedTexture
{
    QString dataRootPath;      //Project data root, the cwDiskCacher root
    cwDiskCacher::Key key;     //The KTX2 entry: UASTC with a full mip chain
    QSize size;                //Level-0 dimensions, known without decoding

    bool isNull() const
    {
        return dataRootPath.isEmpty() || key.id.isEmpty() || size.isEmpty();
    }

    bool operator==(const cwStreamedTexture& other) const
    {
        return dataRootPath == other.dataRootPath
               && key.id == other.key.id
               && key.path == other.key.path
               && key.checksum == other.key.checksum
               && size == other.size;
    }
};

#endif // CWSTREAMEDTEXTURE_H
