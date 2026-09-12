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

/**
 * What a point cloud producer publishes instead of points: where the octree's
 * cache lives, which LAZ file it was built from, the fingerprint that keys its
 * entries, and the manifest that lists its nodes. A plain copyable value —
 * reading node bytes is the renderer's streamer's job, so nothing here touches
 * disk.
 *
 * The cache root is kept cleaned and absolute so every spelling of one
 * directory compares equal. operator== drives the re-publish no-op in
 * cwRenderPointCloud and the node-table reset in cwRHIPointCloud, so two
 * spellings of one root would otherwise throw away every resident node.
 *
 * The manifest is shared rather than copied: the render thread holds the same
 * instance the GUI thread published, and it is const so both may read it.
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
                        std::shared_ptr<const cwPointOctreeManifest> manifest) :
        lazPath(lazPath),
        fingerprint(fingerprint),
        manifest(std::move(manifest)),
        m_cacheRootPath(normalized(cacheRootPath))
    {
    }

    QString lazPath;        //The LAZ file the octree was built from
    QString fingerprint;    //cw::octree::sourceFingerprint of that file
    std::shared_ptr<const cwPointOctreeManifest> manifest;

    //Project root, the cwDiskCacher root, cleaned and absolute
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
               || manifest == nullptr;
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
    static QString normalized(const QString& cacheRootPath)
    {
        //QDir("").absolutePath() answers the working directory, so an empty
        //root stays empty and keeps isNull() true.
        return cacheRootPath.isEmpty() ? QString() : QDir(cacheRootPath).absolutePath();
    }

    QString m_cacheRootPath;
};
