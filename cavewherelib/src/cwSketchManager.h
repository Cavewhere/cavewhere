/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSKETCHMANAGER_H
#define CWSKETCHMANAGER_H

//Qt includes
#include <QHash>
#include <QImage>
#include <QModelIndex>
#include <QObject>
#include <QPointer>
#include <QQmlEngine>
#include <QSet>
#include <QTimer>

//Our includes
#include "CaveWhereLibExport.h"
#include "cwDiskCacher.h"
#include "cwUniqueConnectionChecker.h"

class cwProject;
class cwRegionTreeModel;
class cwTrip;
class cwSketch;
class cwSurveyNoteSketchModel;

/**
 * @brief Rasterises sketch thumbnails into the shared project disk cache and
 * exposes them via the `image://cwcache/...` provider URL.
 *
 * Mirrors cwNoteLiDARManager's lifecycle: walks the region tree to discover
 * trips, subscribes to each trip's sketch model, and tracks per-sketch
 * mutations (strokes reset / stroke-model rows inserted/removed/changed).
 * Changes coalesce through a single-shot timer so a burst of stroke appends
 * produces one write.
 */
class CAVEWHERE_LIB_EXPORT cwSketchManager : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(SketchManager)

public:
    explicit cwSketchManager(QObject* parent = nullptr);
    ~cwSketchManager() override;

    void setProject(cwProject* project);
    void setRegionTreeModel(cwRegionTreeModel* regionTreeModel);

    // Forces a rasterise + cache write for a single sketch. Primarily for
    // tests; production writes happen via the dirty-batch timer.
    Q_INVOKABLE void rasteriseNow(cwSketch* sketch);

    static cwDiskCacher::Key cacheKey(const cwSketch* sketch);
    // `version` is appended as a `?v=<version>` query so QML's Image cache
    // invalidates whenever the icon changes. Pass an empty string to omit.
    static QString cacheUrl(const cwDiskCacher::Key& key, const QString& version);

    // Renders the sketch's strokes into a square QImage of the given edge size.
    static QImage renderIcon(const cwSketch* sketch, int edgePixels = 256);

private slots:
    void handleRegionReset();
    void regionRowsInserted(const QModelIndex& parent, int begin, int end);
    void regionRowsAboutToBeRemoved(const QModelIndex& parent, int begin, int end);

    void sketchRowsInserted(const QModelIndex& parent, int begin, int end);
    void sketchRowsAboutToBeRemoved(const QModelIndex& parent, int begin, int end);

    void sketchDestroyed(QObject* sketchObj);

private:
    void connectTrip(cwTrip* trip);
    void disconnectTrip(cwTrip* trip);

    void connectSketch(cwSketch* sketch);
    void disconnectSketch(cwSketch* sketch);

    void updateIconFromCache(cwSketch* sketch);

    void markDirty(cwSketch* sketch);
    void flushDirty();

    void writeIcon(cwSketch* sketch);

    QDir cacheDir() const;

    QPointer<cwRegionTreeModel> m_regionModel;
    QPointer<cwProject> m_project;

    QSet<cwSketch*> m_dirtySketches;
    QTimer m_flushTimer;

    cwUniqueConnectionChecker m_connectionChecker;
};

#endif // CWSKETCHMANAGER_H
