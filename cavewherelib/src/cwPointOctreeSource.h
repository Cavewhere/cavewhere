// cwPointOctreeSource.h
#pragma once

//Qt includes
#include <QDir>
#include <QString>

//Std includes
#include <memory>

//Our includes
#include "CaveWhereLibExport.h"
#include "cwPointOctreeManifest.h"
#include "cwPointOctreePack.h"

/**
 * What a point cloud producer publishes instead of points: where the octree's
 * cache lives, which LAZ file it was built from, the fingerprint it was built
 * under, and the opened pack that holds its manifest and node bytes. A plain
 * copyable value; the pack is already mapped, so nothing here touches disk.
 *
 * The cache root is kept cleaned and absolute so every spelling of one
 * directory compares equal. operator== drives the re-publish no-op in
 * cwRenderPointCloud and the node-table reset in cwRHIPointCloud, so two
 * spellings of one root would otherwise throw away every resident node.
 *
 * The manifest is the pack's own, shared rather than copied: the render thread
 * holds the same instance the GUI thread published, and it is const so both
 * may read it.
 * One caveat on that sharing: cwPointOctreeManifest::nodeName() fills a mutable
 * parent memo, so the render thread is its only caller on a published manifest.
 * GUI code reads the plain fields and leaves node names alone.
 */
struct CAVEWHERE_LIB_EXPORT cwPointOctreeSource
{
    cwPointOctreeSource() = default;

    cwPointOctreeSource(const QString& cacheRootPath,
                        const QString& lazPath,
                        const QString& fingerprint,
                        std::shared_ptr<const cwPointOctreePack> pack) :
        lazPath(lazPath),
        fingerprint(fingerprint),
        pack(std::move(pack)),
        manifest(manifestOf(this->pack)),
        m_cacheRootPath(normalized(cacheRootPath))
    {
    }

    QString lazPath;        //The LAZ file the octree was built from
    QString fingerprint;    //cw::octree::sourceFingerprint of that file
    std::shared_ptr<const cwPointOctreePack> pack;
    //Built as an alias of pack->manifest(), so it keeps the pack alive; tests
    //may substitute a manifest over the same node bytes
    std::shared_ptr<const cwPointOctreeManifest> manifest;

    //Project root, where .cw_cache lives, cleaned and absolute
    const QString& cacheRootPath() const
    {
        return m_cacheRootPath;
    }

    void setCacheRootPath(const QString& cacheRootPath)
    {
        m_cacheRootPath = normalized(cacheRootPath);
    }

    bool isNull() const
    {
        return m_cacheRootPath.isEmpty() || lazPath.isEmpty() || fingerprint.isEmpty()
               || pack == nullptr || manifest == nullptr;
    }

    //Identity is what the entries are keyed by, never the manifest's address:
    //a rebuilt manifest of the same octree is the same source.
    bool operator==(const cwPointOctreeSource& other) const
    {
        return m_cacheRootPath == other.m_cacheRootPath
               && lazPath == other.lazPath
               && fingerprint == other.fingerprint;
    }

    bool operator!=(const cwPointOctreeSource& other) const
    {
        return !(*this == other);
    }

private:
    static std::shared_ptr<const cwPointOctreeManifest> manifestOf(
        const std::shared_ptr<const cwPointOctreePack>& pack)
    {
        if(pack == nullptr) {
            return nullptr;
        }
        return std::shared_ptr<const cwPointOctreeManifest>(pack, &pack->manifest());
    }

    static QString normalized(const QString& cacheRootPath)
    {
        //QDir("").absolutePath() answers the working directory, so an empty
        //root stays empty and keeps isNull() true.
        return cacheRootPath.isEmpty() ? QString() : QDir(cacheRootPath).absolutePath();
    }

    QString m_cacheRootPath;
};
