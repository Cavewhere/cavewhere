/**************************************************************************
**
**    Copyright (C) 2014 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Qt includes
#include <QFuture>

//Our includes
#include "cwCaptureViewport.h"
#include "cwScale.h"
#include "cwScene.h"
#include "cwOffscreenRenderParameters.h"
#include "cwRenderingSettings.h"
#include "cwCamera.h"
#include "cw3dRegionViewer.h"
#include "cwCaptureCenterline.h"
#include "cwCaptureLeads.h"
#include "cwCaptureLabelPlacer.h"
#include "cwCaptureLeadLines.h"
#include "cwGraphicsImageItem.h"
#include "cwDebug.h"
#include "cwGlobals.h"
#include "cwScaleBarItem.h"
#include "cwCavingRegion.h"
#include "cwCave.h"
#include "cwSurveyNetwork.h"
#include "cwStationPositionLookup.h"

//AsyncFuture includes
#include "asyncfuture.h"

//undef these because micrsoft is fucking retarded...
#ifdef Q_OS_WIN
#undef far
#undef near
#endif

namespace {
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

// The map export renders at a fixed DPI (paperSize * resolution) and must be
// identical regardless of the display it runs on, so each tile is rendered at
// device-pixel-ratio 1 — no Retina supersampling. MSAA (cwRenderingSettings::
// sampleCount) supplies the anti-aliasing the supersample used to provide.
constexpr float kExportDevicePixelRatio = 1.0f;
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
    CaptureRequested(false),
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
        // A render (typically a live preview) is already in flight. Don't drop this
        // request: remember it and re-run in finishCapture() once the current render
        // lands, so a full-res export requested mid-preview still builds its Item.
        CaptureRequested = true;
        return;
    }

    // Every bail-out below must still complete via finishCapture(): cwCaptureManager
    // chains the next layer (and the final save) on finishedCapture(), so a silent
    // return would stall the whole export. abort() clears the re-entrancy flag and
    // signals completion so the manager advances rather than hangs. Capture no
    // longer touches global scene state (the gradient/grid/line-plot are hidden
    // per-tile via hiddenObjectIds), so there is nothing to restore here.
    const auto abort = [this](const QString& reason) {
        qWarning().noquote() << "Map export aborted:" << reason;
        finishCapture();
    };

    // isEmpty() (not isValid()) rejects a zero-area viewport too: QSize(0,0) is
    // "valid", but it would yield zero tiles, and a combine() with no futures
    // never fires its continuation — leaving the capture wedged.
    if(viewport().size().isEmpty()) {
        abort(QStringLiteral("export viewport is empty: %1x%2")
                  .arg(viewport().width()).arg(viewport().height()));
        return;
    }

    cwScene* scene = (view() != nullptr) ? view()->scene() : nullptr;
    if(scene == nullptr) {
        abort(QStringLiteral("export has no render scene (viewer not set)"));
        return;
    }

    CapturingImages = true;

    cwCamera* camera = CaptureCamera;
    cwProjection originalProj = camera->projection();

    QRect viewport = this->viewport();
    QRect cameraViewport = camera->viewport();

    //Flip the viewport so it's in opengl coordinate system instead of qt
    viewport.moveTop(cameraViewport.height() - viewport.bottom());

    double imageScale;
    if(previewCapture()) {
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

    cwProjection croppedProjection = tileProjection(viewport,
                                                    camera->viewport().size(),
                                                    originalProj);

    // Hide the gradient/grid/line-plot so the rendered tiles are transparent-backed,
    // but keep point clouds visible so they appear in the export (EDL-lit, as in the
    // live view). Per-tile visibility override (not a global toggle), so the live 3D
    // view stays interactive and undisturbed throughout the export.
    const QSet<cwRenderObjectId> hiddenObjectIds =
        m_sceneManager.isNull() ? QSet<cwRenderObjectId>{}
                                : m_sceneManager->captureHiddenObjectIds();

    const int sampleCount = cwRenderingSettings::instance()->sampleCount();
    const QMatrix4x4 viewMatrix = camera->viewMatrix();

    // Render every tile offscreen from the resident scene as an independent
    // QFuture<QImage>; combine() below fires once they have all settled. No
    // camera swap and no grabToImage — the offscreen path renders straight from
    // the supplied matrices, so the live view is never disturbed. backgroundColor
    // is left at its transparent default (see cwOffscreenRenderParameters).
    struct Tile {
        QPointF origin;
        QFuture<QImage> future;
    };
    QList<Tile> tiles;
    tiles.reserve(rows * columns);
    for(int column = 0; column < columns; column++) {
        for(int row = 0; row < rows; row++) {

            int x = tileSize.width() * column;
            int y = tileSize.height() * row;

            QSize croppedTileSize = calcCroppedTileSize(tileSize, imageSize, row, column);

            QRect tileViewport(QPoint(x, y), croppedTileSize);
            cwProjection tileProj = tileProjection(tileViewport, imageSize, croppedProjection);

            double originX = column * tileSize.width();
            double originY = onPaperViewport.height() - (row * tileSize.height() + croppedTileSize.height());

            cwOffscreenRenderParameters parameters;
            parameters.viewMatrix = viewMatrix;
            parameters.projectionMatrix = tileProj.matrix();
            parameters.outputSize = croppedTileSize;
            parameters.devicePixelRatio = kExportDevicePixelRatio;
            parameters.sampleCount = sampleCount;
            parameters.hiddenObjectIds = hiddenObjectIds;

            tiles.append(Tile{QPointF(originX, originY), scene->renderOffscreen(parameters)});
        }
    }

    // Defensive: the viewport guard above keeps this unreachable, but a combine()
    // with no futures never fires its continuation, so never leave the capture
    // state machine to depend on a non-empty list having been built.
    if(tiles.isEmpty()) {
        abort(QStringLiteral("export produced no tiles to render"));
        return;
    }

    // AllSettled: a single failed tile must not abort the others, so the export
    // degrades to a missing tile rather than no image at all. Placing every tile
    // here (rather than as each future arrives) keeps placeLabelsAfterTiles()
    // strictly after the last tile lands, with no ordering race against combine.
    // Bind the destination group by value now, at schedule time. The continuation
    // fires after the tiles settle, by which point cwCaptureManager may have flipped
    // setPreviewCapture() for the full-res export; reading previewCapture()/Item at
    // fire-time could select the wrong group and dereference it in addToGroup.
    const bool wasPreview = previewCapture();
    QGraphicsItemGroup* const parent = wasPreview ? PreviewItem : Item;

    auto combine = AsyncFuture::combine(AsyncFuture::AllSettled);
    for(const Tile& tile : tiles) {
        combine << tile.future;
    }
    combine.context(this, [this, tiles, imageScale, wasPreview, parent]() {
        int placed = 0;
        for(const Tile& tile : tiles) {
            const QFuture<QImage>& future = tile.future;
            if(!future.isFinished() || future.resultCount() != 1 || future.result().isNull()) {
                qWarning() << "Map export tile at" << tile.origin << "produced no image; it will be missing from the export";
                continue;
            }

            const QImage image = future.result();
            cwGraphicsImageItem* graphicsImage = new cwGraphicsImageItem(parent);
            graphicsImage->setImage(image);
            graphicsImage->setPos(tile.origin);
            parent->addToGroup(graphicsImage);
            ++placed;
        }

        // Surface a wholly- or partly-empty result: the manager has no failure
        // channel, so without this the user only discovers a blank/holed export
        // by opening the saved file. We still emit finishedCapture() below so the
        // export pipeline completes rather than stalling on this layer.
        if(placed == 0) {
            qWarning() << "Map export image is empty: all" << tiles.size()
                       << "tiles failed to render";
        } else if(placed < tiles.size()) {
            qWarning() << "Map export image is missing" << (tiles.size() - placed)
                       << "of" << tiles.size() << "tiles";
        }

        if(wasPreview) {
            updateBoundingBox();
        }

        // Lay out and create the label overlays now that the tile alpha is
        // available as an obstacle source.
        placeLabelsAfterTiles(parent, imageScale);

        // Clean up. Visibility was overridden per-tile, never globally, so there is
        // no scene state to restore here.
        finishCapture();
    });
}

/**
 * @brief cwCaptureViewport::finishCapture
 *
 * Common completion path for capture(). Clears the re-entrancy flag and either
 * re-runs a request that arrived mid-render (the latest request supersedes this now
 * stale one, so its finishedCapture() is suppressed in favour of the re-run's) or
 * notifies cwCaptureManager that this capture landed.
 */
void cwCaptureViewport::finishCapture()
{
    CapturingImages = false;

    if(CaptureRequested) {
        CaptureRequested = false;
        capture();
        return;
    }

    emit finishedCapture();
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
        return;
    }

    // The parent item's setScale value (inches-per-local-pixel). Driving the
    // font and placer off this — instead of a fixed preview DPI — keeps the
    // rendered glyph the same scene-inch size on both paths.
    const double hiResScale = paperSizeOfItem().width()
                              / (ItemScale * resolution() * viewport().width());
    const double itemSetScale = previewCapture() ? ItemScale : hiResScale;
    const int exportDpi = qMax(1, qRound(1.0 / itemSetScale));

    // Find the parent-coord bounds covering every tile. Each cwGraphicsImageItem
    // has pos() in parent coords and scale() that maps its pixel size to its
    // parent-coord extent (typically 1.0 / devicePixelRatio).
    QRectF parentBounds;
    QList<cwGraphicsImageItem*> tiles;
    const QList<QGraphicsItem*> children = parent->childItems();
    for(QGraphicsItem* child : children) {
        auto* tile = dynamic_cast<cwGraphicsImageItem*>(child);
        if(tile == nullptr) {
            continue;
        }
        tiles.append(tile);
        const qreal tileScale = tile->scale();
        const QImage img = tile->image();
        if(img.isNull()) {
            continue;
        }
        const QRectF tileRect(tile->pos(),
                              QSizeF(img.width() * tileScale, img.height() * tileScale));
        parentBounds = parentBounds.isEmpty() ? tileRect : parentBounds.united(tileRect);
    }
    if(parentBounds.isEmpty()) {
        parentBounds = QRectF(QPointF(0.0, 0.0),
                              QSizeF(viewport().size()) * imageScale);
    }

    // Convert 300-DPI-paper-px constants into the active path's local
    // coords, so cell discretization, label margin, and dot/marker radii
    // all reserve the same scene-inch span in preview and export.
    const double paperPxToLocal = 1.0 / (resolution() * itemSetScale);

    cwCaptureLabelPlacer placer;
    placer.setObstacleBounds(parentBounds, PlacerMaskCellPaperPx * paperPxToLocal);
    placer.setViewportBounds(parentBounds);
    placer.setLabelMarginPaperPx(PlacerLabelMarginPaperPx * paperPxToLocal);
    placer.setAlphaThreshold(cwCaptureLabelPlacer::DefaultAlphaThreshold);

    for(cwGraphicsImageItem* tile : std::as_const(tiles)) {
        const QImage img = tile->image();
        if(img.isNull()) {
            continue;
        }
        placer.addTileAlpha(img, tile->pos(), tile->scale());
    }

    // Build label items now; we hand them the placer + DPI so they can compute
    // glyph rects that match the painter's eventual render.
    CenterlineItem = createCenterlineItem(parent, imageScale);
    if(CenterlineItem != nullptr) {
        CenterlineItem->setExportDpi(exportDpi);
        CenterlineItem->setPaperPxToLocal(paperPxToLocal);
        CenterlineItem->setPlacer(&placer);
    }
    LeadsItem = createLeadsItem(parent, imageScale);
    if(LeadsItem != nullptr) {
        LeadsItem->setExportDpi(exportDpi);
        LeadsItem->setPaperPxToLocal(paperPxToLocal);
        LeadsItem->setPlacer(&placer);
        LeadsItem->setVisible(m_leadsVisible);
    }

    // Seed static dot/marker obstacles BEFORE finalize so labels avoid them.
    if(CenterlineItem != nullptr) {
        const qreal dotHalf = CenterlineItem->stationDotRadius()
                              + StationDotObstacleMarginPaperPx * paperPxToLocal;
        for(const QPointF& pos : CenterlineItem->stationPositions()) {
            placer.addObstacleRect(QRectF(pos.x() - dotHalf, pos.y() - dotHalf,
                                           dotHalf * 2.0, dotHalf * 2.0));
        }
    }
    if(LeadsItem != nullptr) {
        const qreal markerRadius = LeadsItem->markerRadius();
        for(const QPointF& pos : LeadsItem->leadMarkerPositions()) {
            placer.addObstacleRect(QRectF(pos.x() - markerRadius, pos.y() - markerRadius,
                                           markerRadius * 2.0, markerRadius * 2.0));
        }
    }

    placer.finalize();

    // Register centerline legs as soft obstacles so leaders prefer routes
    // that don't visually cut across them. Soft = scoring penalty, not a
    // hard rejection — better to ship a slightly-crossed leader than drop.
    if(CenterlineItem != nullptr) {
        const qreal centerlineThickness =
            cwCaptureCenterline::LinePenWidthPaperPx * paperPxToLocal;
        for(const QLineF& seg : CenterlineItem->lines()) {
            placer.addSoftLineObstacle(seg, centerlineThickness);
        }
    }

    // Place leads first so each placement registers its leader line into
    // the placer; stations placed afterwards then avoid those leaders.
    if(LeadsItem != nullptr) {
        LeadsItem->placeLeadLabels();
    }
    if(CenterlineItem != nullptr) {
        CenterlineItem->placeStationLabels();
    }

    LeadLinesItem = createLeadLinesItem(parent, imageScale, LeadsItem);
    if(LeadLinesItem != nullptr) {
        LeadLinesItem->setVisible(m_leadsVisible);
    }
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

    if(LeadsItem != nullptr) {
        LeadsItem->setVisible(visible);
    }
    if(LeadLinesItem != nullptr) {
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

    if(cwCavingRegion* region = m_sceneManager ? m_sceneManager->cavingRegion() : nullptr) {
        m_scaleBar->setUnitSystem(region->unitSystem());
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
