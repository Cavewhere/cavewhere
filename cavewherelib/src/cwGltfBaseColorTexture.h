#ifndef CWGLTFBASECOLORTEXTURE_H
#define CWGLTFBASECOLORTEXTURE_H

// Qt includes
#include <QHash>
#include <QString>

// Our includes
#include "cwGltfLoader.h"
#include "cwRenderTexturedItems.h"
#include "CaveWhereLibExport.h"

/**
 * Points a render item at the KTX2 form of a glTF material's baseColor texture,
 * held in the project's .cw_cache, and falls back to the decoded RGBA image when
 * there is no entry to point at. The render thread streams the mip levels it
 * wants off that entry, so nothing here uploads whole-texture pixels.
 *
 * One instance covers one .glb file: the constructor hashes the file's bytes
 * once and every texture of that file is keyed off that hash, so an edited scan
 * re-encodes while an untouched one is read straight back. That is what keeps
 * rewarping affordable, since it re-runs the LiDAR task for a note on every
 * declination or station edit — on a warm cache that run decodes nothing.
 *
 * Every member is safe to call from a cwConcurrent worker.
 */
class CAVEWHERE_LIB_EXPORT cwGltfBaseColorTexture
{
public:
    //`parent` is the progress node the file checksum reports under. A null
    //scope leaves the checksum untracked.
    cwGltfBaseColorTexture(const QString& dataRootPath,
                           const QString& gltfFilename,
                           const cwProgressScope& parent = {});

    //`parent` is this texture's own progress node. Only an encode hangs a child
    //off it, so a cache hit leaves it childless.
    void setOn(cwRenderTexturedItems::Item& item,
               const cw::gltf::SceneCPU& scene,
               const cw::gltf::MaterialCPU& material,
               const cwProgressScope& parent = {}) const;

private:
    cwStreamedTexture streamedSource(const cw::gltf::SceneCPU& scene,
                                     int textureIndex,
                                     const cwProgressScope& parent) const;

    QString m_dataRootPath;
    QString m_gltfFilename;
    QString m_fileChecksum;

    //A transient hand-off within a single task run, not a persistent in-memory
    //cache — commit 335671ba removed the process-lifetime scene cache and that
    //decision stands. This instance is stack-local to one run on one worker
    //thread, so it dies with the run and needs no locking. A null descriptor
    //memoizes a failed encode, so a scan the encoder rejects is tried once.
    mutable QHash<int, cwStreamedTexture> m_sourceByIndex;
};

#endif // CWGLTFBASECOLORTEXTURE_H
