/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWCAPTURETILERUN_H
#define CWCAPTURETILERUN_H

//Qt includes
#include <QFuture>
#include <QImage>
#include <QList>
#include <QObject>
#include <QPointF>

//Our includes
#include "cwFutureManagerToken.h"
#include "cwGlobals.h"
#include "cwPhaseJob.h"

class QGraphicsItem;
class QGraphicsItemGroup;

/**
 * Streams one capture run's tile phase (see cwCaptureViewport::capture()).
 *
 * start() wires a settle handler onto every tile future and each tile is
 * processed the moment it lands. A settled QFuture retains its raw rendered
 * image until the last handle to it is destroyed, so holding them all for a
 * single drain would peak at O(page area) raw pixels; each handler instead
 * releases its future right after taking the image, bounding the raw pixels
 * in flight to roughly one render batch plus the compression queue. Preview
 * tiles become raw cwGraphicsImageItems immediately; export tiles are
 * ink-scanned and zlib-compressed on a worker thread and placed as
 * cwCompressedImageItems by that worker's GUI continuation (fully
 * transparent tiles place nothing).
 *
 * Every settle is counted — completed, empty, or canceled — so a failed tile
 * degrades to a missing tile rather than wedging the run, and finished() is
 * emitted exactly once, after the last tile has been processed. For export
 * tiles "processed" means the compression continuation has run, so every
 * tile item exists in tileParent before the signal fires. cancel() deletes
 * the tiles this run already placed and makes the remaining handlers drop
 * their pixels, so a canceled run leaves tileParent as it found it; the run
 * still counts through to finished() — callers key their cancel policy off
 * isCanceled() when it fires.
 *
 * GUI-thread only (it drives a cwPhaseJob). tileParent must stay alive until
 * the run finishes, is canceled, or is destroyed — whichever comes first.
 * After cancel() returns the run never dereferences tileParent again, so the
 * caller may delete it right away (cwCaptureViewport::deleteSceneItems()
 * relies on this). Destruction drops the pending handlers and the cwPhaseJob
 * destructor resolves the job-list row.
 */
class CAVEWHERE_LIB_EXPORT cwCaptureTileRun : public QObject
{
    Q_OBJECT

public:
    //One rendered tile: where it lands in tileParent's coordinates, and the
    //offscreen render that produces its pixels.
    struct Tile {
        QPointF origin;
        QFuture<QImage> future;
    };

    explicit cwCaptureTileRun(QObject* parent = nullptr);

    //Call exactly once, with at least one tile — a run with no tiles never
    //fires a settle handler, so it could never reach finished(). Previews
    //should pass an invalid jobToken so rapid-fire runs don't spam the
    //job list.
    void start(const QList<Tile>& tiles,
               QGraphicsItemGroup* tileParent,
               bool preview,
               cwFutureManagerToken jobToken);

    //Resolves the job-list row immediately, deletes the tile items this run
    //already placed into tileParent, and makes the remaining tile handlers
    //drop their work; the run still counts through to finished(). Once the
    //run has finished its items belong to tileParent, so a later cancel
    //leaves them in place.
    void cancel();
    bool isCanceled() const { return m_canceled; }

signals:
    void finished();

private:
    void handleTileImage(QPointF origin, const QImage& image);
    void tileProcessed(bool placed);

    cwPhaseJob m_job;
    QGraphicsItemGroup* m_tileParent = nullptr;
    QList<QGraphicsItem*> m_placedItems; //placed by an unfinished run; deleted on cancel
    int m_total = 0;
    int m_processed = 0; //every settled tile, placed or not; total emits finished()
    int m_placed = 0;    //tiles that rendered (including blank ones dropped for no ink)
    bool m_preview = false;
    bool m_canceled = false;
};

#endif // CWCAPTURETILERUN_H
