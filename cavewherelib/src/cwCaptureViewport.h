/**************************************************************************
**
**    Copyright (C) 2014 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#ifndef CWVIEWPORTCAPTURE_H
#define CWVIEWPORTCAPTURE_H

//Qt includes
#include <QObject>
#include <QFuture>
#include <QFutureWatcher>
#include <QGraphicsItemGroup>
#include <QPointer>
#include <QSizeF>
#include <QQuickItem>
#include <QQmlEngine>

//Our includes
class cwCamera;
class cwCaptureCenterline;
class cwCaptureLeads;
class cwCaptureLeadLines;
class cwSurveyNetwork;
#include "cwFutureManagerToken.h"
#include "cwScaleBarItem.h"
#include "cwScale.h"
#include "cw3dRegionViewer.h"
#include "cwProjection.h"
#include "cwCaptureItem.h"
#include "cwUnits.h"
#include "cwRegionSceneManager.h"

class cwCaptureViewport : public cwCaptureItem
{
    Q_OBJECT
    QML_NAMED_ELEMENT(CaptureViewport)

    Q_PROPERTY(int resolution READ resolution WRITE setResolution NOTIFY resolutionChanged)
    Q_PROPERTY(QRect viewport READ viewport WRITE setViewport NOTIFY viewportChanged)
    Q_PROPERTY(cwScale* scaleOrtho READ scaleOrtho CONSTANT) //For ortho projections
    Q_PROPERTY(cw3dRegionViewer* view READ view WRITE setView NOTIFY viewChanged)
    Q_PROPERTY(cwRegionSceneManager* sceneManager READ sceneManager WRITE setSceneManager NOTIFY sceneManagerChanged FINAL)
    Q_PROPERTY(QGraphicsItem* previewItem READ previewItem NOTIFY previewItemChanged)
    Q_PROPERTY(QGraphicsItem* fullResolutionItem READ fullResolutionItem NOTIFY fullResolutionItemChanged)
    Q_PROPERTY(double cameraAzimuth READ cameraAzimuth WRITE setCameraAzimuth NOTIFY cameraAzimuthChanged)
    Q_PROPERTY(double cameraPitch READ cameraPitch WRITE setCameraPitch NOTIFY cameraPitchChanged)
    Q_PROPERTY(bool scaleBarVisible READ scaleBarVisible WRITE setScaleBarVisible NOTIFY scaleBarVisibleChanged)
    Q_PROPERTY(bool leadsVisible READ leadsVisible WRITE setLeadsVisible NOTIFY leadsVisibleChanged)

public:

    explicit cwCaptureViewport(QObject *parent = 0);
    virtual ~cwCaptureViewport();

    cw3dRegionViewer* view() const;
    void setView(cw3dRegionViewer* view);

    cwRegionSceneManager *sceneManager() const;
    void setSceneManager(cwRegionSceneManager *newSceneManager);

    // Handle the export label-placement job registers with, so the app's job
    // list surfaces its progress and offers a cancel. Set by cwCaptureManager.
    void setFutureManagerToken(cwFutureManagerToken token);

    // Cancels an in-flight capture run. During the tile phase the tile chain
    // notices the request at the next tile boundary; during label placement
    // the worker-thread future is canceled. Either way the run ends by
    // emitting captureCanceled() instead of finishedCapture(). Does nothing
    // when no capture is running.
    void cancelCapture();

    int resolution() const;
    void setResolution(int resolution);

    QRect viewport() const;
    void setViewport(QRect viewport);

    cwScale* scaleOrtho() const;

    bool previewCapture() const;
    void setPreviewCapture(bool preview);

    QGraphicsItem* previewItem() const;
    QGraphicsItem* fullResolutionItem() const;
    cwScaleBarItem* scaleBarItem() const;

    Q_INVOKABLE void capture(); //This should be called by the

    Q_INVOKABLE void setPaperWidthOfItem(double width);
    Q_INVOKABLE void setPaperHeightOfItem(double height);
    Q_INVOKABLE void setPaperSizePreserveAspect(QSizeF size,
                                                QQuickItem::TransformOrigin dragLocation);

    Q_INVOKABLE void setPositionAfterScale(QPointF position);

    double cameraPitch() const;
    void setCameraPitch(double cameraPitch);

    double cameraAzimuth() const;
    void setCameraAzimuth(double cameraAzimuth);

    QPointF mapToCapture(const cwCaptureViewport *viewport) const;

    bool scaleBarVisible() const;
    void setScaleBarVisible(bool visible);

    bool leadsVisible() const;
    void setLeadsVisible(bool visible);

signals:
    void resolutionChanged();
    void viewportChanged();
    void viewChanged();
    void finishedCapture();
    void captureCanceled();
    void previewItemChanged();
    void fullResolutionItemChanged();
    void transformOriginChanged();
    void cameraPitchChanged();
    void cameraAzimuthChanged();
    void positionAfterScaleChanged();
    void scaleBarVisibleChanged();
    void leadsVisibleChanged();

    void sceneManagerChanged();

private:
    //Properties
    QPointer<cw3dRegionViewer> View; //!<
    QPointer<cwRegionSceneManager> m_sceneManager;
    int Resolution; //!<
    cwScale* ScaleOrtho; //!<
    double ItemScale;
    QRect Viewport;
    bool PreviewCapture;
    cwUnits::LengthUnit PaperUnit;
    QQuickItem::TransformOrigin TransformOrigin; //!<
    double CameraAzimuth; //!<
    double CameraPitch; //!<

    bool CapturingImages;
    int NumberOfImagesProcessed;
    int Columns;
    int Rows;
    QSize TileSize;

    //Scene state information
    cwCamera* CaptureCamera;

    QGraphicsItemGroup* PreviewItem; //This is the preview item
    QGraphicsItemGroup* Item; //This is the full resultion item
    cwScaleBarItem* m_scaleBar;
    bool m_scaleBarVisible = true;

    cwProjection tileProjection(QRectF tileViewport,
                                QSizeF imageSize,
                                const cwProjection& originalProjection) const;

    QSize calcCroppedTileSize(QSize tileSize, QSize imageSize, int row, int column) const;

    void setImageScale(double scale);
    void updateTransformForItem(QGraphicsItem* item, double scale) const;
    void updateBoundingBox();
    void deleteSceneItems();
    cwSurveyNetwork buildCenterlineNetwork() const;
    cwCaptureCenterline* createCenterlineItem(QGraphicsItemGroup* parent, double imageScale) const;
    cwCaptureLeads* createLeadsItem(QGraphicsItemGroup* parent, double imageScale) const;
    cwCaptureLeadLines* createLeadLinesItem(QGraphicsItemGroup* parent, double imageScale, cwCaptureLeads* leadsPeer) const;
    // Runs the label-placement stage (distance-transform build + per-label
    // placement) on a worker thread and, on the GUI thread when it finishes,
    // builds the leader lines and emits finishedCapture() (or captureCanceled()
    // if it was aborted). Returns immediately; does not block the GUI.
    void placeLabelsAfterTiles(QGraphicsItemGroup* parent, double imageScale);

    // In-flight worker-thread label placement (see placeLabelsAfterTiles).
    // Held so it can be canceled on cancelCapture()/destruction. Canceling a
    // finished/default future is a no-op.
    QFuture<void> m_labelPlacementFuture;
    // Drives the GUI-thread continuation off QFutureWatcher::finished, which
    // fires only once the worker has actually unwound. Its canceled signal
    // fires as soon as cancel() is called — while the worker is still running
    // — so nothing may key off it (see the continuation-contract test in
    // test_cwLabelPlacementThreading.cpp).
    QFutureWatcher<void> m_labelPlacementWatcher;
    // The tile-rendering phase's job-list future (see capture()). Backed by a
    // GUI-thread cwPhaseJob owned by the run; held here so cancelCapture()
    // can resolve the job-list entry immediately instead of at the next tile
    // boundary, and so deleteSceneItems() can resolve it on destruction.
    // Canceling a finished/default future is a no-op.
    QFuture<void> m_tileGrabFuture;
    // Set by cancelCapture() so the tile-grab chain (which polls it per tile)
    // can end the run at the next tile boundary. Reset by capture().
    bool m_cancelRequested = false;
    // Whether the in-flight run is a preview, snapshotted when the run starts.
    // The live previewCapture() flag can be flipped mid-run (the manager sets
    // it to false right before requesting the export), so mid-run code must
    // use this snapshot, not the live flag.
    bool m_runIsPreview = false;
    // Set when capture() is requested while a preview run is still in flight:
    // the preview is canceled and, once it has fully stopped, capture() is
    // re-run in the caller's mode. Cleared by cancelCapture() so an explicit
    // cancel also drops the queued restart.
    bool m_captureAgainWhenDone = false;
    cwFutureManagerToken m_futureManagerToken;
    // Grab-phase totals for the kept (inked) export tiles, before and after
    // zlib compression. Reset by capture(); reported by the opt-in
    // CW_PROFILE_CAPTURE log so real exports show their compression ratio.
    qint64 m_rawTileBytes = 0;
    qint64 m_compressedTileBytes = 0;

private slots:
    // void capturedImage(QImage image, int id);
    void updateTransformForItems();
    void updateItemsPosition();
    void updateScaleBarScale();
    void updateScaleBarGeometry();

private:
    cwCaptureCenterline* CenterlineItem;
    cwCaptureLeads* LeadsItem;
    cwCaptureLeadLines* LeadLinesItem;
    bool m_leadsVisible = false;
};

/**
* @brief cwCaptureViewport::resolution
* @return
*/
inline int cwCaptureViewport::resolution() const {
    return Resolution;
}

/**
* @brief class::scale
* @return
*/
inline cwScale* cwCaptureViewport::scaleOrtho() const {
    return ScaleOrtho;
}

/**
* @brief cwScreenCaptureManager::viewport
* @return
*/
inline QRect cwCaptureViewport::viewport() const {
    return Viewport;
}

/**
 * @brief previewCapture
 * @return
 */
inline bool cwCaptureViewport::previewCapture() const {
    return PreviewCapture;
}

/**
 * @brief cwCaptureViewport::setPreviewCapture
 * @param preview
 */
inline void cwCaptureViewport::setPreviewCapture(bool preview)
{
    PreviewCapture = preview;
}

/**
* @brief cwViewportCapt::previewItem
* @return
*/
inline QGraphicsItem* cwCaptureViewport::previewItem() const {
    return PreviewItem;
}

/**
* @brief cwCaptureViewport::fullResolutionItem
* @return
*/
inline QGraphicsItem* cwCaptureViewport::fullResolutionItem() const {
    return Item;
}

inline cwScaleBarItem* cwCaptureViewport::scaleBarItem() const {
    return m_scaleBar;
}

inline bool cwCaptureViewport::scaleBarVisible() const {
    return m_scaleBarVisible;
}

inline bool cwCaptureViewport::leadsVisible() const {
    return m_leadsVisible;
}

/**
* @brief cwCaptureItem::cameraPitch
* @return
*/
inline double cwCaptureViewport::cameraPitch() const {
    return CameraPitch;
}

/**
* @brief cwCaptureViewport::cameraAzimuth
* @return
*/
inline double cwCaptureViewport::cameraAzimuth() const {
    return CameraAzimuth;
}

inline cwRegionSceneManager *cwCaptureViewport::sceneManager() const
{
    return m_sceneManager;
}

#endif // CWVIEWPORTCAPTURE_H
