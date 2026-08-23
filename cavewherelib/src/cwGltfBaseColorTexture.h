#ifndef CWGLTFBASECOLORTEXTURE_H
#define CWGLTFBASECOLORTEXTURE_H

// Qt includes
#include <QString>

// Our includes
#include "cwGltfLoader.h"
#include "cwRenderTexturedItems.h"
#include "CaveWhereLibExport.h"

/**
 * Puts a glTF material's baseColor texture on a render item, preferring the
 * GPU-compressed form held in the project's .cw_cache and falling back to the
 * decoded RGBA image.
 *
 * One instance covers one .glb file: the constructor hashes the file's bytes
 * once and every texture of that file is keyed off that hash, so an edited scan
 * re-encodes while an untouched one is read straight back. That is what keeps
 * rewarping affordable, since it re-runs the LiDAR task for a note on every
 * declination or station edit.
 *
 * Every member is safe to call from a cwConcurrent worker.
 */
class CAVEWHERE_LIB_EXPORT cwGltfBaseColorTexture
{
public:
    cwGltfBaseColorTexture(const QString& dataRootPath, const QString& gltfFilename);

    void setOn(cwRenderTexturedItems::Item& item,
               const cw::gltf::SceneCPU& scene,
               const cw::gltf::MaterialCPU& material) const;

private:
    cwCompressedTexture compressedTexture(const QImage& image, int textureIndex) const;

    QString m_dataRootPath;
    QString m_gltfFilename;
    QString m_fileChecksum;
};

#endif // CWGLTFBASECOLORTEXTURE_H
