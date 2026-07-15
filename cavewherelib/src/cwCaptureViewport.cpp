/**************************************************************************
**
**    Copyright (C) 2014 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Qt includes
#include <QPen>
#include <QUuid>
#include <QQuickItemGrabResult>
#include <QElapsedTimer>
#include <QPromise>

//Std includes
#include <memory>
#include <cstring>

//Our includes
#include "cwCaptureViewport.h"
#include "cwScale.h"
#include "cwScene.h"
#include "cwCamera.h"
#include "cw3dRegionViewer.h"
#include "cwCaptureCenterline.h"
#include "cwCaptureLeads.h"
#include "cwCaptureLabelPlacer.h"
#include "cwCaptureLeadLines.h"
#include "cwGraphicsImageItem.h"
#include "cwPhaseJob.h"
#include "cwCompressedImage.h"
#include "cwCompressedImageItem.h"
#include "cwMemoryUsage.h"
#include "cwDebug.h"
#include "cwGlobals.h"
#include "cwScaleBarItem.h"
#include "cwCavingRegion.h"
#include "cwCave.h"
#include "cwSurveyNetwork.h"
#include "cwStationPositionLookup.h"
#include "cwConcurrent.h"
#include "cwLabelPlacementControl.h"

//undef these because micrsoft is fucking retarded...
#ifdef Q_OS_WIN
#undef far
#undef near
#endif

namespace {
// Set this env var to log a one-shot per-export breakdown of the label-
// placement stages — see placeLabelsAfterTiles. Off by default; when unset the
// profiling timers never start, so there is no cost.
constexpr const char* ProfileCaptureEnvVar = "CW_PROFILE_CAPTURE";

// A rendered tile's alpha channel, snapshotted on the GUI thread and fed to
// the placer on the worker thread. Preview tiles arrive as a raw QImage;
// export tiles as the cwCompressedImage their cwCompressedImageItem holds.
// Both are implicitly shared, so this copies handles; the worker reads but
// never mutates them.
struct TileAlphaInput {
    QImage            image;      // preview tiles
    cwCompressedImage compressed; // export tiles
    QPointF pos;
    qreal   scale = 1.0;
};

// Everything the worker thread needs to build the distance transform and place
// every label, gathered on the GUI thread (all GUI-item construction and
// geometry reads happen there) so the worker touches no GUI state except the
// two label items it mutates in place. That in-place mutation is safe because
// nothing on the GUI thread paints or deletes those items while the worker
// runs: they are hidden until the continuation reveals them (previews live in
// the rendered scene, so painting them mid-run would race), and the
// CapturingImages guard — cleared only once the worker has unwound — blocks
// re-entrant capture() from deleting them.
// See cwCaptureViewport::placeLabelsAfterTiles.
struct LabelPlacementInput {
    cwCaptureLabelPlacer placer;
    QRectF  parentBounds;
    qreal   cellSizeLocal    = 0.0;
    qreal   labelMarginLocal = 0.0;
    QVector<TileAlphaInput>       tiles;
    QVector<QRectF>               obstacleRects; // station dots + lead markers, before finalize()
    QVector<QPair<QLineF, qreal>> softSegments;  // centerline legs, after finalize()
    cwCaptureCenterline* centerline = nullptr;
    cwCaptureLeads*      leads      = nullptr;
    bool profile     = false;
    // Grab-phase tile compression totals, copied in for the profile log so a
    // real export reports its actual compression ratio.
    qint64 rawTileBytes        = 0;
    qint64 compressedTileBytes = 0;
};

// Z-values define stacking order of overlays drawn on top of the rendered map tiles.
// LeadLeaders sits below the tiles (z = 0) so it shows through transparent
// regions and is hidden behind opaque cave-passage ink.
constexpr qreal LeadLeadersZValue = -100.0;
constexpr qreal CenterlineZValue = 1500.0;
constexpr qreal LeadsZValue = 1600.0;
constexpr qreal ScaleBarZValue = 2000.0;

// Placer mask: cellSize is the obstacle/DT grid resolution in paper-pixel
// units (smaller = more accurate, more memory). LabelMargin is the per-label
// clearance added around glyph ink so labels don't abut passage edges.
// StationDotMargin pads each station dot when seeded as an obstacle.
constexpr qreal PlacerMaskCellPaperPx = 2.0;
constexpr qreal PlacerLabelMarginPaperPx = 3.0;
constexpr qreal StationDotObstacleMarginPaperPx = 1.0;

// CW_PROFILE_CAPTURE memory-trace cadence: log the process footprint every
// this many grabbed tiles / placed labels, so an export that dies of memory
// still shows WHICH phase was climbing and what it was proportional to.
constexpr int ProfileTileLogInterval  = 100;
constexpr int ProfileLabelLogInterval = 5000;

double toMB(qint64 bytes) { return bytes / (1024.0 * 1024.0); }
}

cwCaptureViewport::cwCaptureViewport(QObject *parent) :
    cwCaptureItem(parent),
    Resolution(300),
    ScaleOrtho(new cwScale(this)),
    ItemScale(1.0),
    PreviewCapture(true),
    PaperUnit(cwUnits::Inches),
    TransformOrigin(QQuickItem::TopLeft),
    CapturingImages(false),
    NumberOfImagesProcessed(0),
    Columns(0),
    Rows(0),
    TileSize(1024, 1024),
    CaptureCamera(new cwCamera(this)),
    PreviewItem(nullptr),
    Item(nullptr),
    m_scaleBar(new cwScaleBarItem()),
    CenterlineItem(nullptr),
    LeadsItem(nullptr),
    LeadLinesItem(nullptr)
{
    connect(ScaleOrtho, &cwScale::scaleChanged, this, &cwCaptureViewport::updateTransformForItems);
    connect(this, &cwCaptureViewport::positionOnPaperChanged, this, &cwCaptureViewport::updateItemsPosition);
    connect(this, &cwCaptureViewport::rotationChanged, this, &cwCaptureViewport::updateTransformForItems);
    connect(this, &cwCaptureViewport::boundingBoxChanged, this, &cwCaptureViewport::updateScaleBarGeometry);

    m_scaleBar->setZValue(ScaleBarZValue);
    m_scaleBar->setVisible(false);

    updateScaleBarScale();
}

cwCaptureViewport::~cwCaptureViewport()
{
    deleteSceneItems();
}

/**
* @brief cwScreenCaptureManager::setView
* @param view
*/
void cwCaptureViewport::setView(cw3dRegionViewer* view) {
    if(View != view) {
        View = view;
        emit viewChanged();
    }
}

/**
* @brief cwCaptureViewport::setResolution
* @param resolution
*/
void cwCaptureViewport::setResolution(int resolution) {
    if(Resolution != resolution) {
        Resolution = resolution;
        emit resolutionChanged();
    }
}

/**
 * @brief cwScreenCaptureManager::setViewport
 * @param viewport
 *
 * This is the capturing viewport in opengl. This is the area that will be captured
 * and saved by the manager. The rectangle should be in pixels.
 */
void cwCaptureViewport::setViewport(QRect viewport) {
    if(Viewport != viewport) {
        Viewport = viewport;
        emit viewportChanged();

        if(!View.isNull()) {
            cwCamera* camera = View->camera();
            CaptureCamera->setProjection(camera->projection());
            CaptureCamera->setViewport(camera->viewport());
            CaptureCamera->setViewMatrix(camera->viewMatrix());
        }
    }
}

/**
 * @brief cwCaptureViewport::capture
 */
void cwCaptureViewport::capture()
{
    if(CapturingImages) {
        if(m_runIsPreview) {
            // A preview run is still in flight (its label placement is async)
            // and its worker owns the label items, so we can't tear it down
            // here. Cancel it and restart capture() — in the caller's
            // now-current mode — once it has fully stopped. Silently dropping
            // the request would leave an export run waiting on tiles that
            // were never captured.
            cancelCapture();
            m_captureAgainWhenDone = true;
        }
        return;
    }
    CapturingImages = true;
    m_cancelRequested = false;
    m_runIsPreview = previewCapture();
    m_rawTileBytes = 0;
    m_compressedTileBytes = 0;

    //QRect::isEmpty() (unlike QSize::isValid(), which accepts 0x0) rejects a
    //zero-area viewport — which would otherwise build an empty tile-job list
    //and crash dereferencing its first job.
    if(viewport().isEmpty()) {
        qWarning() << "Viewport isn't valid for export:" << viewport();
        //Abort as a canceled run so neither this viewport nor a manager run
        //driving it is left waiting on a signal that would never come.
        CapturingImages = false;
        emit captureCanceled();
        return;
    }

    cwScene* scene = view()->scene();
    cwCamera* camera = CaptureCamera;
    cwProjection originalProj = camera->projection();

    QRect viewport = this->viewport();
    QRect cameraViewport = camera->viewport();

    //Flip the viewport so it's in opengl coordinate system instead of qt
    viewport.moveTop(cameraViewport.height() - viewport.bottom());

    double imageScale;
    if(m_runIsPreview) {
        if(PreviewItem != NULL) {
            delete PreviewItem;
            PreviewItem = nullptr;
            CenterlineItem = nullptr;
            LeadsItem = nullptr;
            LeadLinesItem = nullptr;
        }
        PreviewItem = new QGraphicsItemGroup();
        previewItemChanged();

        imageScale = 1.0;
    } else {
        if(Item != NULL) {
            delete Item;
            Item = nullptr;
            CenterlineItem = nullptr;
            LeadsItem = nullptr;
            LeadLinesItem = nullptr;
        }
        Item = new QGraphicsItemGroup();
        fullResolutionItemChanged();

        imageScale = ItemScale * resolution();
    }
    // Label items (centerline / leads / lead leaders) are deferred to
    // placeLabelsAfterTiles() once tile rendering has finished, so their
    // layout can use the rendered tile alpha as obstacle data.

    //Updates the scale for the items
    updateTransformForItems();

    QPointF previewItemPosition = PreviewItem->pos();

    QRectF onPaperViewport = QRectF(QPoint(previewItemPosition.x() * imageScale, previewItemPosition.y() * imageScale),
                                    QSizeF(viewport.width() * imageScale, viewport.height() * imageScale));

    QSize tileSize = TileSize;
    QSize imageSize = onPaperViewport.size().toSize(); //QSize(viewport.width() * imageScale, viewport.height() * imageScale);

    int columns = imageSize.width() / tileSize.width();
    int rows = imageSize.height() / tileSize.height();

    if(imageSize.width() % tileSize.width() > 0) { columns++; }
    if(imageSize.height() % tileSize.height() > 0) { rows++; }

    NumberOfImagesProcessed = 0;
    Columns = columns;
    Rows = rows;

    cwProjection croppedProjection = tileProjection(viewport,
                                                    camera->viewport().size(),
                                                    originalProj);

    struct CaptureJob {
        int id;
        QPointF origin;
        QRect viewport;
        cwProjection tileProj;
        QMatrix4x4 viewMatrix;
    };

    struct CaptuteRunData {
        cwCamera* oldCamera;
        cwCamera* captureCamera = new cwCamera();
        QList<CaptureJob> CaptureJobs;
        // Backs the tile-rendering job-list entry. GUI-thread only: started,
        // ticked per grabbed tile, and finished EXPLICITLY by both exit
        // branches of the grab chain (completion and cancel). cwPhaseJob's
        // destructor safety net never fires here: the recursive grab lambdas
        // keep this run data alive through a shared_ptr self-reference, so it
        // is never destroyed.
        cwPhaseJob tileGrabJob;
    };

    auto runData = std::make_shared<CaptuteRunData>();
    runData->oldCamera = scene->camera();

    // Surface the tile-rendering phase as a job (name + progress + runtime) in
    // the app's job list — on a whole-page export it is thousands of
    // sequential grabToImage calls and usually the longest visible wait. The
    // job runs for every capture so the completion/cancel paths below stay
    // unconditional, but previews pass an invalid token so they don't register
    // (same rationale as the placement job: they fire rapidly and would spam
    // the job list).
    runData->tileGrabJob.start(QStringLiteral("Rendering map tiles"),
                               rows * columns,
                               m_runIsPreview ? cwFutureManagerToken()
                                              : m_futureManagerToken);
    m_tileGrabFuture = runData->tileGrabJob.future();

    //These are recursive lambdas, so we need to put this in a shared pointer
    auto capturedImage = std::make_shared<std::function<void (const CaptureJob& data, const QImage& image)>>();

    auto nextJob = [runData, this, capturedImage]() {
        auto job = runData->CaptureJobs.at(NumberOfImagesProcessed);
        runData->captureCamera->setProjection(job.tileProj);
        runData->captureCamera->setViewMatrix(job.viewMatrix);
        runData->captureCamera->setViewport(job.viewport);

        auto grabResult = view()->grabToImage(job.viewport.size());
        connect(grabResult.get(), &QQuickItemGrabResult::ready, this,
                [capturedImage, job, grabResult, this]() mutable {
            const QImage image = grabResult->image();

            // Release the grab result NOW instead of letting this connection's
            // closure retain it until the viewport is destroyed — that
            // retention held every tile's ~17 MB result alive and leaked
            // ~18 MB per tile across an export (hundreds of GB on a
            // whole-page run, regardless of tile discard/compression). We are
            // inside the result's own ready() emission and its shared pointer
            // deletes with a plain `delete` (qquickitemgrabresult.cpp), so
            // dropping the last reference here would destroy the signal
            // sender mid-emit: park it in a queued no-op instead — the event
            // loop drops it (and with it this connection, whose sender dies)
            // on the next pass. `image` stays valid; it shares the pixel data.
            QMetaObject::invokeMethod(this, [result = std::move(grabResult)]() {},
                                      Qt::QueuedConnection);

            (*capturedImage)(job, image);
        });
    };

    *capturedImage = [runData, nextJob, imageScale, this](const CaptureJob& job, const QImage& image) {
        Q_ASSERT(CapturingImages);

        if(m_cancelRequested) {
            // Canceled during the tile phase (there is no placement future to
            // cancel yet). End the run at this tile boundary: restore the
            // scene, drop the remaining tiles, and skip label placement.
            // cancelCapture() already canceled the tile-grab future (which
            // resolves its job-list entry); finishing the job settles it.
            runData->tileGrabJob.finish();
            NumberOfImagesProcessed = 0;
            Rows = 0;
            Columns = 0;
            m_sceneManager->setCapturing(false);
            view()->scene()->setCamera(runData->oldCamera);
            CapturingImages = false;
            if(m_captureAgainWhenDone) {
                // This run was superseded (typically an export superseding
                // the initial preview); start the new run instead of
                // announcing a cancel nobody asked for.
                m_captureAgainWhenDone = false;
                capture();
                return;
            }
            emit captureCanceled();
            return;
        }

        QGraphicsItemGroup* parent = m_runIsPreview ? PreviewItem : Item;

        if(m_runIsPreview) {
            // Preview tiles are few and screen-sized, and repaint interactively
            // in the layout scene, so they stay raw. (They are also always
            // kept: the preview's bounding box derives from its tile items.)
            cwGraphicsImageItem* graphicsImage = new cwGraphicsImageItem(parent);
            QImage correctedImage = image;
            correctedImage.setDevicePixelRatio(1.0);
            graphicsImage->setImage(correctedImage);
            graphicsImage->setPos(job.origin);
            graphicsImage->setScale(1.0 / image.devicePixelRatio());
            parent->addToGroup(graphicsImage);
        } else {
            // Export tiles are what ran whole-page exports out of application
            // memory: O(page area) of raw pixels held until saveScene renders
            // them, exactly once. So: normalize to a known QRgb format (the
            // RHI readback behind grabToImage is already ARGB32_Premultiplied
            // on Metal — the conversion only fires on foreign backends whose
            // byte order a raw QRgb scan would misread), drop fully
            // transparent tiles (on a sparse whole-cave page that is MOST of
            // them — the ink scan exits at the first opaque pixel), and store
            // the survivors zlib-compressed. Transparent premultiplied pixels
            // are zero runs, so inked tiles shrink by orders of magnitude.
            const QImage normalized =
                (image.format() == QImage::Format_ARGB32
                 || image.format() == QImage::Format_ARGB32_Premultiplied)
                    ? image
                    : image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
            auto tileHasInk = [](const QImage& img) {
                if(img.format() == QImage::Format_ARGB32_Premultiplied) {
                    // Premultiplied guarantees alpha 0 ⟹ an all-zero pixel
                    // word, so the overwhelmingly common case on a sparse
                    // page — a fully transparent tile — reduces to comparing
                    // each row against a zero row instead of per-pixel alpha
                    // reads on the GUI thread.
                    const qsizetype rowBytes = qsizetype(img.width())
                                               * qsizetype(sizeof(QRgb));
                    const QByteArray zeroRow(rowBytes, '\0');
                    for(int y = 0; y < img.height(); y++) {
                        if(memcmp(img.constScanLine(y), zeroRow.constData(),
                                  size_t(rowBytes)) != 0) {
                            return true;
                        }
                    }
                    return false;
                }
                for(int y = 0; y < img.height(); y++) {
                    const QRgb* row = reinterpret_cast<const QRgb*>(img.constScanLine(y));
                    for(int x = 0; x < img.width(); x++) {
                        if(qAlpha(row[x]) != 0) {
                            return true;
                        }
                    }
                }
                return false;
            };
            if(tileHasInk(normalized)) {
                auto* tileItem = new cwCompressedImageItem(parent);
                tileItem->setImage(normalized);
                tileItem->setPos(job.origin);
                tileItem->setScale(1.0 / image.devicePixelRatio());
                parent->addToGroup(tileItem);
                m_rawTileBytes        += tileItem->compressedImage().rawSizeInBytes();
                m_compressedTileBytes += tileItem->compressedImage().compressedData().size();
            }
        }

        //For debugging tiles
        // double scale = 1.0  / image.devicePixelRatio();
        // QGraphicsRectItem* rectItem = new QGraphicsRectItem(parent);
        // QRectF tileRect = graphicsImage->mapToItem(rectItem, graphicsImage->boundingRect()).boundingRect();
        // rectItem->setPen(QPen(Qt::red));
        // rectItem->setRect(tileRect);
        // QGraphicsSimpleTextItem* textItem = new QGraphicsSimpleTextItem(parent);
        // textItem->setText(QString("Id:%1").arg(job.id));
        // textItem->setPen(QPen(Qt::red));
        // textItem->setPos(tileRect.center());

        NumberOfImagesProcessed++;
        runData->tileGrabJob.setProgressValue(NumberOfImagesProcessed);

        // Memory trace for whole-page export debugging: a run that dies of
        // memory never reaches the end-of-run profile line, so emit the
        // trajectory as the grab progresses — footprint should stay flat
        // while the compressed total grows slowly; whichever counter climbs
        // with it is the leak.
        if(!m_runIsPreview
           && NumberOfImagesProcessed % ProfileTileLogInterval == 0
           && qEnvironmentVariableIsSet(ProfileCaptureEnvVar)) {
            qDebug() << "[cwCaptureViewport profile] tile grab —"
                     << "processed" << NumberOfImagesProcessed << "/" << (Rows * Columns)
                     << "kept" << parent->childItems().size()
                     << "tileRawMB" << toMB(m_rawTileBytes)
                     << "tileCompressedMB" << toMB(m_compressedTileBytes)
                     << "footprintMB" << cwMemoryUsage::physFootprintMB();
        }

        if(NumberOfImagesProcessed == Rows * Columns) {
            //Finished capturing tiles. CapturingImages stays true through the
            //async label placement below so re-entrant capture() calls are
            //blocked until it finishes — its continuation clears the flag.
            //Settle the tile-rendering job; the placement job takes over from
            //here (placeLabelsAfterTiles registers it).
            runData->tileGrabJob.finish();
            NumberOfImagesProcessed = 0;
            Rows = 0;
            Columns = 0;

            if(m_runIsPreview) {
                updateBoundingBox();
            }

            // Tile rendering is done, so restore the scene/camera now. Label
            // placement uses the snapshotted tile images and CaptureCamera, not
            // the live scene, so the 3D view is interactive again while the
            // placement runs on a worker thread.
            m_sceneManager->setCapturing(false);
            view()->scene()->setCamera(runData->oldCamera);

            // Lay out the label overlays on a worker thread now that the tile
            // alpha is available as an obstacle source. This emits
            // finishedCapture() (or captureCanceled()) from its GUI-thread
            // continuation — it does NOT block here.
            placeLabelsAfterTiles(parent, imageScale);
        } else {
            //Next image to capture
            nextJob();
        }
    };

    //Queue the jobs
    for(int column = 0; column < columns; column++) {
        for(int row = 0; row < rows; row++) {

            int x = tileSize.width() * column;
            int y = tileSize.height() * row;

            QSize croppedTileSize = calcCroppedTileSize(tileSize, imageSize, row, column);

            QRect tileViewport(QPoint(x, y), croppedTileSize);
            cwProjection tileProj = tileProjection(tileViewport, imageSize, croppedProjection);

            int id = row * columns + column;

            double originX = column * tileSize.width();
            double originY = onPaperViewport.height() - (row * tileSize.height() + croppedTileSize.height());
            QPointF origin(originX, originY);

            QRect viewport = QRect(QPoint(), croppedTileSize);

            runData->CaptureJobs.append(
                CaptureJob{
                    id,
                    origin,
                    viewport,
                    tileProj,
                    camera->viewMatrix()
                });
        }
    }

    //Initial settings
    m_sceneManager->setCapturing(true);
    view()->scene()->setCamera(runData->captureCamera);

    nextJob();

    // view()->update();
}

/**
 * @brief cwCaptureViewport::setPaperWidthOfItem
 * @param width
 *
 * This sets the width in the paper units of how big the catpure is. This will mantain the
 * aspect of the catpure
 */
void cwCaptureViewport::setPaperWidthOfItem(double width)
{
    //Using a fuzzy compare, to prevent recursive stack overflow
    //Mathmatically using "!=" should work, but there's a little bit of a fuzzy
    //to the scale values
    if(!qFuzzyCompare(paperSizeOfItem().width(), width)) {

        double scale =  width / (double)viewport().width();
        double height = viewport().height() * scale;

        setPaperSizeOfItem(QSizeF(width, height));
        setImageScale(scale);
    }
}

/**
 * @brief cwCaptureViewport::setPaperHeightOfItem
 * @param height
 *
 * This sets the height in the paper units of how big the capture is. This will mantain the
 * aspect of the catpure
 */
void cwCaptureViewport::setPaperHeightOfItem(double height)
{
    //Using a fuzzy compare, to prevent recursive stack overflow
    //See comment in setPaperWidthOfItem for more details
    if(!qFuzzyCompare(paperSizeOfItem().height(), height)) {
        double scale =  height / (double)viewport().height();
        double width = viewport().width() * scale;

        setPaperSizeOfItem(QSizeF(width, height));
        setImageScale(scale);
    }
}

void cwCaptureViewport::setPaperSizePreserveAspect(QSizeF size,
                                                   QQuickItem::TransformOrigin dragLocation)
{
    //This prevents extra signals from this function
    QSignalBlocker blockThis(this);

    auto viewport = this->viewport();
    double hScale = size.height() / (double)viewport.height();
    double wScale = size.width() / (double)viewport.width();

    //Finds the corner that shouldn't move with the paper resize
    QQuickItem::TransformOrigin fixedCorner = [](QQuickItem::TransformOrigin dragLocation) {
        switch(dragLocation) {
        case QQuickItem::TransformOrigin::TopLeft:
            return QQuickItem::TransformOrigin::BottomRight;
        case QQuickItem::TransformOrigin::TopRight:
            return QQuickItem::TransformOrigin::BottomLeft;
        case QQuickItem::TransformOrigin::BottomLeft:
            return QQuickItem::TransformOrigin::TopRight;
        case QQuickItem::TransformOrigin::BottomRight:
            return QQuickItem::TransformOrigin::TopLeft;
        default:
            qDebug() << "DragLocation:" << dragLocation << "isn't supported" << LOCATION;
            return QQuickItem::TransformOrigin::TopLeft;
        }
    }(dragLocation);

    auto cornerPosition = [this](QQuickItem::TransformOrigin corner) {
        const auto box = boundingBox();
        switch(corner) {
        case QQuickItem::TransformOrigin::TopLeft:
            return box.topLeft();
        case QQuickItem::TransformOrigin::TopRight:
            return box.topRight();
        case QQuickItem::TransformOrigin::BottomLeft:
            return box.bottomLeft();
        case QQuickItem::TransformOrigin::BottomRight:
            return box.bottomRight();
        default:
            qDebug() << "Corner:" << corner << "isn't supported" << LOCATION;
            return QPointF();
        }
    };

    auto oldCorner = cornerPosition(fixedCorner);

    double scale = qMax(wScale, hScale);
    // QSizeF newScale = size * scale;

    if(size.width() > size.height()) {
        setPaperSizeOfItem(QSizeF(scale * viewport.width(), size.height()));
    } else {
        setPaperSizeOfItem(QSizeF(size.width(), scale * viewport.height()));
    }
    setImageScale(scale);

    blockThis.unblock();

    //Make sure the opposite corner of the dragLocation stays fixed
    auto newCorner = cornerPosition(fixedCorner);
    auto positionDelta = oldCorner - newCorner;

    setPositionOnPaper(positionOnPaper() + positionDelta);
    emit paperSizeOfItemChanged();
    emit boundingBoxChanged();
}

/**
 * @brief cwCaptureViewport::setPositionAfterScale
 * @param posiiton
 *
 * This function should be called after scaling the capture viewport.
 * This is a special case for positioning the viewport. This will
 * cause the capture group to position other captures correctly. After the
 * scale, the CaptureItemInteraction changes the scale, it may need position
 * the capture, and this positioning must be handled differently then
 * a normal setPositionOnPaper(). Specifially, a secondary capture scaling
 * has no effect on the parent primary capture. In this case, the secondary
 * will update primary capture.
 */
void cwCaptureViewport::setPositionAfterScale(QPointF position)
{
    //Block signals from this object while setting the position. This
    //vill be replace with positionAfterScaleChanged() signal
    blockSignals(true);
    setPositionOnPaper(position);
    updateItemsPosition();
    blockSignals(false);

    emit positionAfterScaleChanged();
    emitPositionOnPaper(); //Emit position changed!
}

/**
 * @brief cwCaptureViewport::makeItemsNull
 *
 * This function is to prevent stall pointers of the Item and PreviewItem
 *
 * This function is called by the CaptureManager when scene is destroyed
 */
void cwCaptureViewport::deleteSceneItems()
{
    // A worker-thread label placement may still be touching CenterlineItem /
    // LeadsItem (owned by the Item / PreviewItem groups deleted below). Cancel
    // it and wait for the worker to unwind before freeing those items, or it
    // would use-after-free. QFuture::waitForFinished() blocks this thread
    // WITHOUT spinning a nested event loop (unlike AsyncFuture::waitForFinished),
    // so there is no re-entrancy risk. The placement loop polls the cancel once
    // per label; the longest uncancelable stretch is a single window's
    // distance-transform build, so this wait is bounded by one window, not the
    // page.
    m_labelPlacementFuture.cancel();
    m_labelPlacementFuture.waitForFinished();

    // Also resolve a tile-phase job-list row. The grab chain's shared_ptr
    // self-reference means its cwPhaseJob is never destroyed, so a viewport
    // destroyed mid-grab (e.g. ~cwCaptureManager deleting its layers without
    // cancelCapture()) would otherwise leave a stuck "Rendering map tiles"
    // row forever — the job list removes rows on the future's cancel.
    m_tileGrabFuture.cancel();

    if(PreviewItem != nullptr) {
        delete PreviewItem;
        PreviewItem = nullptr;
        CenterlineItem = nullptr;
        LeadsItem = nullptr;
        LeadLinesItem = nullptr;
    }

    if(Item != nullptr) {
        delete Item;
        Item = nullptr;
        CenterlineItem = nullptr;
        LeadsItem = nullptr;
        LeadLinesItem = nullptr;
    }

    delete m_scaleBar;
    m_scaleBar = nullptr;
}

cwSurveyNetwork cwCaptureViewport::buildCenterlineNetwork() const
{
    cwSurveyNetwork network;

    if(m_sceneManager.isNull()) {
        return network;
    }

    cwCavingRegion* region = m_sceneManager->cavingRegion();
    if(region == nullptr) {
        return network;
    }

    const QList<cwCave*> caves = region->caves();
    for(cwCave* cave : caves) {
        if(cave == nullptr) {
            continue;
        }

        const cwSurveyNetwork caveNetwork = cave->network();
        const cwStationPositionLookup stationLookup = cave->stationPositionLookup();
        const QStringList stations = caveNetwork.stations();
        for(const QString& station : stations) {
            if(stationLookup.hasPosition(station)) {
                network.setPosition(station, stationLookup.position(station));
            }
        }

        for(const QString& station : stations) {
            const QStringList neighbors = caveNetwork.neighbors(station);
            for(const QString& neighbor : neighbors) {
                network.addShot(station, neighbor);
            }
        }
    }

    qDebug() << "Build network station empty:" << network.isEmpty();

    return network;
}

cwCaptureCenterline* cwCaptureViewport::createCenterlineItem(QGraphicsItemGroup* parent, double imageScale) const
{
    if(parent == nullptr) {
        return nullptr;
    }

    auto* centerline = new cwCaptureCenterline(parent);
    centerline->setZValue(CenterlineZValue);
    centerline->setCamera(CaptureCamera);
    centerline->setViewport(viewport());
    centerline->setImageScale(imageScale);
    centerline->setNetwork(buildCenterlineNetwork());

    return centerline;
}

cwCaptureLeads* cwCaptureViewport::createLeadsItem(QGraphicsItemGroup* parent, double imageScale) const
{
    if(parent == nullptr) {
        return nullptr;
    }

    auto* leads = new cwCaptureLeads(parent);
    leads->setZValue(LeadsZValue);
    leads->setCamera(CaptureCamera);
    leads->setViewport(viewport());
    leads->setImageScale(imageScale);

    if(!m_sceneManager.isNull()) {
        leads->setRegion(m_sceneManager->cavingRegion());
    }

    leads->setVisible(m_leadsVisible);

    return leads;
}

void cwCaptureViewport::placeLabelsAfterTiles(QGraphicsItemGroup* parent, double imageScale)
{
    if(parent == nullptr) {
        // Nothing to place, but the capture still has to finish so the manager
        // can move on and save.
        CapturingImages = false;
        emit finishedCapture();
        return;
    }

    // Opt-in profiling of the label-placement stages. The timers only run when
    // CW_PROFILE_CAPTURE is set; otherwise this is free.
    const bool profile = qEnvironmentVariableIsSet(ProfileCaptureEnvVar);

    // The parent item's setScale value (inches-per-local-pixel). Driving the
    // font and placer off this — instead of a fixed preview DPI — keeps the
    // rendered glyph the same scene-inch size on both paths.
    const double hiResScale = paperSizeOfItem().width()
                              / (ItemScale * resolution() * viewport().width());
    const double itemSetScale = m_runIsPreview ? ItemScale : hiResScale;
    const int exportDpi = qMax(1, qRound(1.0 / itemSetScale));

    // Snapshot every tile (both go to the worker as implicitly shared
    // handles) and find the parent-coord bounds covering them. Previews hold
    // raw cwGraphicsImageItem tiles; exports hold cwCompressedImageItem tiles
    // whose zlib blob the placer decompresses once, lazily. Each item's pos()
    // is in parent coords and scale() maps its pixel size to its parent-coord
    // extent (typically 1.0 / devicePixelRatio).
    QRectF parentBounds;
    QVector<TileAlphaInput> tileInputs;
    const QList<QGraphicsItem*> children = parent->childItems();
    tileInputs.reserve(children.size());
    for(QGraphicsItem* child : children) {
        TileAlphaInput tileInput;
        if(auto* rawTile = dynamic_cast<cwGraphicsImageItem*>(child)) {
            const QImage img = rawTile->image();
            if(img.isNull()) {
                continue;
            }
            tileInput.image = img;
        } else if(auto* compressedTile = dynamic_cast<cwCompressedImageItem*>(child)) {
            const cwCompressedImage compressed = compressedTile->compressedImage();
            if(compressed.isNull()) {
                continue;
            }
            tileInput.compressed = compressed;
        } else {
            continue;
        }
        tileInput.pos   = child->pos();
        tileInput.scale = child->scale();
        const QSize sizePixels = tileInput.image.isNull()
                                     ? tileInput.compressed.size()
                                     : tileInput.image.size();
        const QRectF tileRect(tileInput.pos,
                              QSizeF(sizePixels) * tileInput.scale);
        parentBounds = parentBounds.isEmpty() ? tileRect : parentBounds.united(tileRect);
        tileInputs.append(tileInput);
    }
    // Anchor the bounds to the full page rect: empty (fully transparent)
    // tiles are discarded at capture time, so the tile union alone could
    // shrink to the inked region and silently change the placement grid,
    // viewport clamp, and cull. United with the tiles it also tolerates
    // sub-pixel tile overhang from edge-tile crop rounding.
    const QRectF fullPageRect(QPointF(0.0, 0.0),
                              QSizeF(viewport().size()) * imageScale);
    parentBounds = parentBounds.isEmpty() ? fullPageRect
                                          : parentBounds.united(fullPageRect);

    // Convert 300-DPI-paper-px constants into the active path's local
    // coords, so cell discretization, label margin, and dot/marker radii
    // all reserve the same scene-inch span in preview and export.
    const double paperPxToLocal = 1.0 / (resolution() * itemSetScale);

    // Gather everything the worker thread needs. The input (and the placer it
    // owns by value) outlives this call; the worker and the continuation both
    // hold the input.
    auto input = std::make_shared<LabelPlacementInput>();
    input->parentBounds     = parentBounds;
    input->cellSizeLocal    = PlacerMaskCellPaperPx * paperPxToLocal;
    input->labelMarginLocal = PlacerLabelMarginPaperPx * paperPxToLocal;
    input->profile          = profile;

    input->tiles = std::move(tileInputs);
    input->rawTileBytes        = m_rawTileBytes;
    input->compressedTileBytes = m_compressedTileBytes;

    if(profile && !m_runIsPreview) {
        qDebug() << "[cwCaptureViewport profile] tile grab done —"
                 << "tiles kept" << input->tiles.size()
                 << "tileRawMB" << toMB(input->rawTileBytes)
                 << "tileCompressedMB" << toMB(input->compressedTileBytes)
                 << "footprintMB" << cwMemoryUsage::physFootprintMB();
    }

    // Build label items on the GUI thread — QGraphicsItem construction must not
    // happen on the worker. We hand them the DPI + scale so they can compute
    // glyph rects that match the painter's eventual render; their request
    // building / placement application runs later on the worker, mutating only
    // their own data. Both items stay HIDDEN until the continuation reveals
    // them: for previews the parent group is already in the rendered scene, and
    // painting an item while the worker sorts/writes its data vectors would be
    // a data race.
    CenterlineItem = createCenterlineItem(parent, imageScale);
    if(CenterlineItem != nullptr) {
        CenterlineItem->setExportDpi(exportDpi);
        CenterlineItem->setPaperPxToLocal(paperPxToLocal);
        CenterlineItem->setVisible(false);
    }
    LeadsItem = createLeadsItem(parent, imageScale);
    if(LeadsItem != nullptr) {
        LeadsItem->setExportDpi(exportDpi);
        LeadsItem->setPaperPxToLocal(paperPxToLocal);
        LeadsItem->setVisible(false);
    }
    input->centerline = CenterlineItem;
    input->leads      = LeadsItem;

    // Gather static dot/marker obstacles (seeded before finalize) from the
    // items' geometry — cheap GUI-thread reads, so the worker needs no GUI
    // access to build them.
    if(CenterlineItem != nullptr) {
        const qreal dotHalf = CenterlineItem->stationDotRadius()
                              + StationDotObstacleMarginPaperPx * paperPxToLocal;
        const QVector<QPointF> stationPositions = CenterlineItem->stationPositions();
        input->obstacleRects.reserve(input->obstacleRects.size() + stationPositions.size());
        for(const QPointF& pos : stationPositions) {
            input->obstacleRects.append(QRectF(pos.x() - dotHalf, pos.y() - dotHalf,
                                               dotHalf * 2.0, dotHalf * 2.0));
        }
    }
    if(LeadsItem != nullptr) {
        const qreal markerRadius = LeadsItem->markerRadius();
        const QVector<QPointF> markerPositions = LeadsItem->leadMarkerPositions();
        input->obstacleRects.reserve(input->obstacleRects.size() + markerPositions.size());
        for(const QPointF& pos : markerPositions) {
            input->obstacleRects.append(QRectF(pos.x() - markerRadius, pos.y() - markerRadius,
                                               markerRadius * 2.0, markerRadius * 2.0));
        }
    }

    // Gather centerline legs as soft obstacles (registered after finalize) so
    // leaders prefer routes that don't visually cut across them.
    if(CenterlineItem != nullptr) {
        const qreal centerlineThickness =
            cwCaptureCenterline::LinePenWidthPaperPx * paperPxToLocal;
        const QVector<QLineF> legs = CenterlineItem->lines();
        input->softSegments.reserve(legs.size());
        for(const QLineF& seg : legs) {
            input->softSegments.append({seg, centerlineThickness});
        }
    }

    // Run the distance-transform build + per-label placement on a worker
    // thread. The placer is CPU-only and its grids are reentrant, so a single
    // worker needs no locking. Progress + cancel flow through the QPromise.
    m_labelPlacementFuture = cwConcurrent::run([input](QPromise<void>& promise) {
        cwCaptureLabelPlacer& placer = input->placer;

        QElapsedTimer stageTimer;
        qint64 measureMs = 0;
        qint64 placeMs = 0;

        placer.setObstacleBounds(input->parentBounds, input->cellSizeLocal);
        placer.setViewportBounds(input->parentBounds);
        placer.setLabelMarginPaperPx(input->labelMarginLocal);
        placer.setAlphaThreshold(cwCaptureLabelPlacer::DefaultAlphaThreshold);

        // These record obstacle sources; the distance transform itself is
        // built lazily per placement window inside placeAll, so its memory is
        // bounded by one window (plus halo) — never the whole page.
        for(const TileAlphaInput& tile : std::as_const(input->tiles)) {
            if(!tile.image.isNull()) {
                placer.addTileAlpha(tile.image, tile.pos, tile.scale);
            } else {
                placer.addCompressedTileAlpha(tile.compressed, tile.pos, tile.scale);
            }
        }
        for(const QRectF& rect : std::as_const(input->obstacleRects)) {
            placer.addObstacleRect(rect);
        }
        placer.finalize();

        for(const QPair<QLineF, qreal>& soft : std::as_const(input->softSegments)) {
            placer.addSoftLineObstacle(soft.first, soft.second);
        }

        if(promise.isCanceled()) { return; }

        // Cancel + progress hooks: request building polls the cancel (text
        // measurement over tens of thousands of stations takes real time) and
        // placeAll additionally ticks progress once per label, so a canceled
        // export bails mid-run.
        int processed = 0;
        cwLabelPlacementControl control;
        control.isCanceled = [&promise]() { return promise.isCanceled(); };
        control.labelProcessed = [&promise, &processed, &placer,
                                  profile = input->profile]() {
            processed++;
            promise.setProgressValue(processed);
            // Placement-phase memory trace: window builds and tile masks are
            // the phase's only growing allocations — if the footprint climbs
            // faster than maskMB, the leak is elsewhere.
            if(profile && processed % ProfileLabelLogInterval == 0) {
                const cwCaptureLabelPlacer::Stats& s = placer.stats();
                qDebug() << "[cwCaptureViewport profile] placing —"
                         << "labels" << processed
                         << "windowsBuilt" << s.windowsBuilt
                         << "windowCellsBuilt" << s.windowCellsBuilt
                         << "windowRasterMs" << s.windowRasterMs
                         << "windowDtMs" << s.windowDtMs
                         << "tileMasksBuilt" << s.tileMasksBuilt
                         << "tileMaskMB" << toMB(s.tileMaskBytes)
                         << "footprintMB" << cwMemoryUsage::physFootprintMB();
            }
        };

        // Measure every label up front, leads first: within each DT window
        // placeAll keeps request order, so a window's leads place before its
        // stations and register their leaders as obstacles for them.
        if(input->profile) { stageTimer.start(); }
        QVector<cwCaptureLabelPlacer::LabelRequest> requests;
        int leadRequestCount = 0;
        if(input->leads != nullptr) {
            requests = input->leads->buildLabelRequests(control);
            leadRequestCount = requests.size();
        }
        if(input->centerline != nullptr) {
            requests += input->centerline->buildLabelRequests(control);
        }
        if(input->profile) {
            measureMs = stageTimer.elapsed();
            qDebug() << "[cwCaptureViewport profile] labels measured —"
                     << "requests" << requests.size()
                     << "measure(ms)" << measureMs
                     << "footprintMB" << cwMemoryUsage::physFootprintMB();
        }

        if(promise.isCanceled()) { return; }

        promise.setProgressRange(0, qMax(1, int(requests.size())));

        if(input->profile) { stageTimer.restart(); }
        const QVector<cwCaptureLabelPlacer::Placement> placements =
            placer.placeAll(requests, control);
        if(input->leads != nullptr) {
            input->leads->applyPlacements(placements.mid(0, leadRequestCount));
        }
        if(input->centerline != nullptr) {
            input->centerline->applyPlacements(placements.mid(leadRequestCount));
        }
        if(input->profile) {
            placeMs = stageTimer.elapsed();
            const cwCaptureLabelPlacer::Stats& s = placer.stats();
            qDebug() << "[cwCaptureViewport profile] export label placement (worker) —"
                     << "tiles" << input->tiles.size()
                     << "tileRawMB" << toMB(input->rawTileBytes)
                     << "tileCompressedMB" << toMB(input->compressedTileBytes)
                     << "tileMasksBuilt" << s.tileMasksBuilt
                     << "tileMaskMB" << toMB(s.tileMaskBytes)
                     << "bounds" << input->parentBounds.size()
                     << "gridCells" << s.gridCells
                     << "| measure(ms)" << measureMs
                     << "placement(ms)" << placeMs
                     << "| windowsBuilt" << s.windowsBuilt
                     << "windowCellsBuilt" << s.windowCellsBuilt
                     << "maxWindowCells" << s.maxWindowCells
                     << "windowRasterMs" << s.windowRasterMs
                     << "windowDtMs" << s.windowDtMs
                     << "| placeCalls" << s.placeCalls
                     << "placed" << s.placed
                     << "culled" << s.culledByViewport
                     << "noCandidate" << s.noCandidate
                     << "cellsTried" << s.cellsTried
                     << "dtClearedCells" << s.dtClearedCells
                     << "placedLabelChecks" << s.placedLabelChecks
                     << "softObstacleChecks" << s.softObstacleChecks
                     << "lineObstacleChecks" << s.lineObstacleChecks
                     << "| gridCellsVisited placed" << placer.placedGridCellsVisited()
                     << "line" << placer.lineGridCellsVisited()
                     << "soft" << placer.softGridCellsVisited()
                     << "| footprintMB" << cwMemoryUsage::physFootprintMB()
                     << LOCATION;
        }
    });

    // Surface the export placement as a job (progress + a cancel affordance) in
    // the app's job list. Previews run off-thread too, but silently — they fire
    // rapidly and would spam the job list.
    if(!m_runIsPreview && m_futureManagerToken.isValid()) {
        m_futureManagerToken.addJob({m_labelPlacementFuture,
                                     QStringLiteral("Placing export labels")});
    }

    // Back on the GUI thread when the worker has actually unwound: reveal the
    // now-placed label items, build the leader lines (they depend on the
    // placed leads), and finish the capture. This keys off
    // QFutureWatcher::finished — NOT an AsyncFuture cancel callback, whose
    // canceled notification fires the moment cancel() is called, while the
    // worker may still be mutating the items; clearing CapturingImages then
    // would let a re-entrant capture() delete those items under the worker.
    // `input` is captured to keep the placer alive until the results are
    // applied here.
    connect(&m_labelPlacementWatcher, &QFutureWatcher<void>::finished, this,
            [this, input, parent, imageScale]() {
        CapturingImages = false;

        if(m_captureAgainWhenDone) {
            // This run was superseded (typically an export superseding the
            // initial preview). The worker has unwound, so the restarted
            // capture() may safely rebuild the items; the superseded run
            // emits no signals — the caller is waiting on the new run's.
            m_captureAgainWhenDone = false;
            capture();
            return;
        }

        if(m_labelPlacementFuture.isCanceled()) {
            // Canceled: leave the partially placed items hidden; the run's
            // output is discarded by the manager.
            emit captureCanceled();
            return;
        }

        if(CenterlineItem != nullptr) {
            CenterlineItem->setVisible(true);
        }
        if(LeadsItem != nullptr) {
            LeadsItem->setVisible(m_leadsVisible);
        }
        LeadLinesItem = createLeadLinesItem(parent, imageScale, LeadsItem);
        if(LeadLinesItem != nullptr) {
            LeadLinesItem->setVisible(m_leadsVisible);
        }
        emit finishedCapture();
    }, Qt::SingleShotConnection);
    m_labelPlacementWatcher.setFuture(m_labelPlacementFuture);
}

void cwCaptureViewport::setFutureManagerToken(cwFutureManagerToken token)
{
    m_futureManagerToken = token;
}

void cwCaptureViewport::cancelCapture()
{
    if(!CapturingImages) {
        // Nothing in flight; don't latch a cancel against a future run.
        return;
    }
    // An explicit cancel also drops any queued restart (capture() re-latches
    // m_captureAgainWhenDone after calling this when it supersedes a preview).
    m_captureAgainWhenDone = false;
    m_cancelRequested = true;
    // Resolve the tile-rendering job-list entry right away; the run itself
    // still ends at the next tile boundary (the grab chain polls
    // m_cancelRequested), which also finishes the promise.
    m_tileGrabFuture.cancel();
    m_labelPlacementFuture.cancel();
}

cwCaptureLeadLines* cwCaptureViewport::createLeadLinesItem(QGraphicsItemGroup* parent, double imageScale, cwCaptureLeads* leadsPeer) const
{
    if(parent == nullptr) {
        return nullptr;
    }

    auto* lines = new cwCaptureLeadLines(parent);
    lines->setZValue(LeadLeadersZValue);
    lines->setLeads(leadsPeer);

    const QSizeF localSize = QSizeF(viewport().size()) * imageScale;
    lines->setBoundingRect(QRectF(QPointF(0.0, 0.0), localSize));
    lines->setVisible(m_leadsVisible);

    return lines;
}

/**
 * @brief cwScreenCaptureManager::tileProjection
 * @param viewport
 * @param originalProjection
 * @return
 *
 * This will take original viewport and original projection and find the
 * tile projection matrix using the tileViewport. The tileViewport
 * should be a sub rectangle of the original viewport. This function
 * will work with orthognal and perspective projections
 */
cwProjection cwCaptureViewport::tileProjection(QRectF tileViewport,
                                               QSizeF imageSize,
                                               const cwProjection &originalProjection) const
{
    double originalProjectionWidth = originalProjection.right() - originalProjection.left();
    double originalProjectionHeight = originalProjection.top() - originalProjection.bottom();

    double left = originalProjection.left() + originalProjectionWidth
                                                  * (tileViewport.left() / imageSize.width());
    double right = left + originalProjectionWidth * tileViewport.width() / imageSize.width();
    double bottom = originalProjection.bottom() + originalProjectionHeight
                                                      * (tileViewport.top() / imageSize.height());
    double top = bottom + originalProjectionHeight * tileViewport.height() / imageSize.height();

    cwProjection projection;
    switch(originalProjection.type()) {
    case cwProjection::Perspective:
    case cwProjection::PerspectiveFrustum:
        projection.setFrustum(left, right,
                              bottom, top,
                              originalProjection.near(), originalProjection.far());
        return projection;
    case cwProjection::Ortho:
        projection.setOrtho(left, right,
                            bottom, top,
                            originalProjection.near(), originalProjection.far());
        return projection;
    default:
        qWarning() << "Can't return tile matrix because original matrix isn't a orth or perspectiveMatrix" << LOCATION;
        break;
    }
    return projection;
}

/**
 * @brief cwScreenCaptureManager::croppedTile
 * @param tileSize
 * @param imageSize
 * @param row
 * @param column
 * @return Return the tile size of row and column
 *
 * This may crop the tile, if the it goes beyond the imageSize
 */
QSize cwCaptureViewport::calcCroppedTileSize(QSize tileSize, QSize imageSize, int row, int column) const
{
    QSize croppedTileSize = tileSize;

    bool exactEdgeX = imageSize.width() % tileSize.width() == 0;
    bool exactEdgeY = imageSize.height() % tileSize.height() == 0;
    int lastColumnIndex = imageSize.width() / tileSize.width();
    int lastRowIndex = imageSize.height() / tileSize.height();


    Q_ASSERT(column <= lastColumnIndex);
    Q_ASSERT(row <= lastRowIndex);

    if(column == lastColumnIndex && !exactEdgeX)
    {
        //Edge tile, crop it
        double tilesInImageX = imageSize.width() / (double)tileSize.width();
        double extraX = tilesInImageX - floor(tilesInImageX);
        double imageWidthCrop = ceil(tileSize.width() * extraX);
        croppedTileSize.setWidth(imageWidthCrop);
    }

    if(row == lastRowIndex && !exactEdgeY)
    {
        //Edge tile, crop it
        double tilesInImageY = imageSize.height() / (double)tileSize.height();
        double extraY = tilesInImageY - floor(tilesInImageY);
        double imageHeightCrop = ceil(tileSize.height() * extraY);
        croppedTileSize.setHeight(imageHeightCrop);
    }

    return croppedTileSize;
}

/**
 * @brief cwCaptureViewport::setImageScale
 * @param scale
 *
 * This sets the scaling for the preview item and the full resolution image.
 *
 * If an orthognal projection is being used the scaleOrtho is also update.
 */
void cwCaptureViewport::setImageScale(double scale)
{
    if(ItemScale != scale) {
        if(CaptureCamera->projection().type() == cwProjection::Ortho) {
            double meterToPaperUnit = cwUnits::convert(1.0, cwUnits::Meters, PaperUnit);
            ScaleOrtho->setScale(scale * CaptureCamera->pixelsPerMeter() * 1.0 / meterToPaperUnit);
        } else {
            ItemScale = scale;
            updateTransformForItems();
        }
    }
}

/**
 * @brief cwCaptureViewport::updateTransformForItem
 * @param item - The item that needs it's transformation updated
 * @param scale - The new scale of the item
 *
 * This update the scale and rotation for the item
 */
void cwCaptureViewport::updateTransformForItem(QGraphicsItem *item, double scale) const
{
    QTransform transform;
    transform.scale(scale, scale);

    QSizeF size = paperSizeOfItem();
    double centerX = size.width() / scale * 0.5;
    double centerY = size.height() / scale * 0.5;

    //The center in pixel coordinates
    QPointF center = QPointF(centerX, centerY);

    transform.translate(center.x(), center.y());
    transform.rotate(rotation());
    transform.translate(-center.x(), -center.y());

    item->setTransform(transform);
}

/**
 * @brief cwCaptureViewport::updateBoundingBox
 *
 * This will update the bounding box for the viewport capture.
 *
 * This is useful for displaying annotation, and interactions ontop of the item
 * in qml.
 */
void cwCaptureViewport::updateBoundingBox()
{
    QTransform transform = previewItem()->transform();
    QRectF paperRect = previewItem()->sceneBoundingRect();
    if(paperRect.isEmpty()) {
        // Label items (centerline / leads / lead leaders) are added by
        // placeLabelsAfterTiles() once tile rendering completes, so the group
        // is empty during capture start. Fall back to the expected viewport-
        // sized rect so the QML captureItem has correct bounds immediately.
        paperRect = QRectF(previewItem()->pos(),
                           QSizeF(viewport().size()) * ItemScale);
    }
    setBoundingBox(paperRect);
}

/**
 * @brief cwCaptureViewport::updateScaleForItems
 *
 * This updates the scale for QGraphicsItems (Preview Item and the Full resolution item)
 */
void cwCaptureViewport::updateTransformForItems()
{
    double meterToPaperUnit = cwUnits::convert(1.0, cwUnits::Meters, PaperUnit);
    if(CaptureCamera->projection().type() == cwProjection::Ortho) {
        ItemScale = ScaleOrtho->scale() * (1.0 / CaptureCamera->pixelsPerMeter()) * (meterToPaperUnit);
    }

    double paperWidth = viewport().size().width() * ItemScale;
    setPaperWidthOfItem(paperWidth);

    if(previewItem() != nullptr) {
        updateTransformForItem(previewItem(), ItemScale);
        updateBoundingBox();
    }

    if(fullResolutionItem() != nullptr) {
        double hiResScale = paperSizeOfItem().width() / (ItemScale * resolution() * viewport().width());
        updateTransformForItem(fullResolutionItem(), hiResScale);
        fullResolutionItem()->setPos(previewItem()->pos());
    }

    updateScaleBarScale();
    updateScaleBarGeometry();
}

/**
 * @brief cwCaptureViewport::updateItemsPosition
 * @param positionOnPaper
 * This sets the position of the viewport capture on the paper. This is
 * in paper units.
 */
void cwCaptureViewport::updateItemsPosition()
{
    if(previewItem() != nullptr) {
        previewItem()->setPos(positionOnPaper());
    }

    if(fullResolutionItem() != nullptr) {
        fullResolutionItem()->setPos(positionOnPaper());
    }

    updateBoundingBox();

}

void cwCaptureViewport::setScaleBarVisible(bool visible)
{
    if(m_scaleBarVisible == visible) {
        return;
    }

    m_scaleBarVisible = visible;

    m_scaleBar->setVisible(visible);

    emit scaleBarVisibleChanged();
}

void cwCaptureViewport::setLeadsVisible(bool visible)
{
    if(m_leadsVisible == visible) {
        return;
    }

    m_leadsVisible = visible;

    // While a capture is in flight the label items are deliberately hidden
    // (the worker is mutating their data — see placeLabelsAfterTiles); the
    // continuation applies the current m_leadsVisible when it reveals them.
    if(LeadsItem != nullptr && !CapturingImages) {
        LeadsItem->setVisible(visible);
    }
    if(LeadLinesItem != nullptr && !CapturingImages) {
        LeadLinesItem->setVisible(visible);
    }

    emit leadsVisibleChanged();
}

void cwCaptureViewport::updateScaleBarScale()
{
    double ratio = 0.0;
    if(CaptureCamera->projection().type() == cwProjection::Ortho) {
        ratio = scaleOrtho()->scale();
    }

    if(!qFuzzyCompare(ratio + 1.0, m_scaleBar->scaleRatio() + 1.0)) {
        m_scaleBar->setScaleRatio(ratio);
        if(!m_scaleBarVisible) {
            m_scaleBar->setVisible(false);
        }
    }
}

void cwCaptureViewport::updateScaleBarGeometry()
{
    QRectF borderRect = boundingBox();
    if(!borderRect.isValid() || borderRect.isNull()) {
        return;
    }

    m_scaleBar->setBorderRect(borderRect);
    if(!m_scaleBarVisible) {
        m_scaleBar->setVisible(false);
    }
}

/**
* @brief cwScreenCaptureManager::view
* @return
*/
cw3dRegionViewer* cwCaptureViewport::view() const {
    return View;
}

/**
* @brief cwCaptureItem::setCameraPitch
* @param cameraPitch - The camera's pitch (degrees). This should come out of the turntableinteraction
*/
void cwCaptureViewport::setCameraPitch(double cameraPitch) {
    if(CameraPitch != cameraPitch) {
        CameraPitch = cameraPitch;
        emit cameraPitchChanged();
    }
}

/**
* @brief cwCaptureViewport::setCameraAzimuth
* @param cameraAzimuth - The camera's azimuth (degrees). This should come out of the turn table interaction
*/
void cwCaptureViewport::setCameraAzimuth(double cameraAzimuth) {
    if(CameraAzimuth != cameraAzimuth) {
        CameraAzimuth = cameraAzimuth;
        emit cameraAzimuthChanged();
    }
}

/**
 * @brief cwCaptureViewport::mapWorldToPaper
 * @param worldCoordinate
 * @return Returns the mapped coordinate
 *
 * Converts point in this object's paper  coordinates to the viewport's paper coordinates.
 */
QPointF cwCaptureViewport::mapToCapture(const cwCaptureViewport* viewport) const
{
    Q_ASSERT(qFuzzyCompare(scaleOrtho()->scale(), viewport->scaleOrtho()->scale()));

    QPointF topLeftToOriginPixels = viewport->CaptureCamera->project(QVector3D(0.0, 0.0, 0.0)) - viewport->viewport().topLeft();
    QPointF paperOrigin = viewport->previewItem()->mapToScene(topLeftToOriginPixels);

    QPointF thisTopLeftToOriginPixels = CaptureCamera->project(QVector3D(0.0, 0.0, 0.0)) - this->viewport().topLeft();
    QPointF thisOrigin = positionOnPaper() - previewItem()->mapToScene(thisTopLeftToOriginPixels);

    return paperOrigin + thisOrigin;
}



void cwCaptureViewport::setSceneManager(cwRegionSceneManager *newSceneManager)
{
    if (m_sceneManager == newSceneManager) {
        return;
    }
    m_sceneManager = newSceneManager;
    emit sceneManagerChanged();
}
