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
#include <QGraphicsItemGroup>
#include <QPointer>
#include <QSizeF>
#include <QQuickItem>

//Our includes
class cwScale;
class cw3dRegionViewer;
class cwCamera;
#include "cwProjection.h"
#include "cwCaptureItem.h"
#include "cwUnits.h"

class cwCaptureViewport : public cwCaptureItem
{
    Q_OBJECT

    Q_PROPERTY(int resolution READ resolution WRITE setResolution NOTIFY resolutionChanged)
    Q_PROPERTY(QRect viewport READ viewport WRITE setViewport NOTIFY viewportChanged)
    Q_PROPERTY(cwScale* scaleOrtho READ scaleOrtho CONSTANT) //For ortho projections
    Q_PROPERTY(cw3dRegionViewer* view READ view WRITE setView NOTIFY viewChanged)
    Q_PROPERTY(QGraphicsItem* previewItem READ previewItem NOTIFY previewItemChanged)
    Q_PROPERTY(QGraphicsItem* fullResolutionItem READ fullResolutionItem NOTIFY fullResolutionItemChanged)

    public:

    explicit cwCaptureViewport(QObject *parent = 0);
    virtual ~cwCaptureViewport();

    cw3dRegionViewer* view() const;
    void setView(cw3dRegionViewer* view);

    int resolution() const;
    void setResolution(int resolution);

    QRect viewport() const;
    void setViewport(QRect viewport);

    cwScale* scaleOrtho() const;

    bool previewCapture() const;
    void setPreviewCapture(bool preview);

    QGraphicsItem* previewItem() const;
    QGraphicsItem* fullResolutionItem() const;

    Q_INVOKABLE void capture(); //This should be called by the

    Q_INVOKABLE void setPaperWidthOfItem(double width);
    Q_INVOKABLE void setPaperHeightOfItem(double height);

signals:
    void resolutionChanged();
    void viewportChanged();
    void viewChanged();
    void finishedCapture();
    void previewItemChanged();
    void fullResolutionItemChanged();
    void transformOriginChanged();

public slots:

private:
    //Properties
    QPointer<cw3dRegionViewer> View; //!<
    int Resolution; //!<
    cwScale* ScaleOrtho; //!<
    double ItemScale;
    QRect Viewport;
    bool PreviewCapture;
    cwUnits::LengthUnit PaperUnit;
    QQuickItem::TransformOrigin TransformOrigin; //!<

    bool CapturingImages;
    int NumberOfImagesProcessed;
    int Columns;
    int Rows;
    QSize TileSize;
    QHash<int, QPointF> IdToOrigin;

    //Scene state information
    cwCamera* CaptureCamera;

    QGraphicsItemGroup* PreviewItem; //This is the preview item
    QGraphicsItemGroup* Item; //This is the full resultion item

    cwProjection tileProjection(QRectF tileViewport,
                                QSizeF imageSize,
                                const cwProjection& originalProjection) const;

    QSize calcCroppedTileSize(QSize tileSize, QSize imageSize, int row, int column) const;

    void setImageScale(double scale);
    void updateTransformForItem(QGraphicsItem* item, double scale) const;
    void updateBoundingBox();
    void deleteSceneItems();

private slots:
    void capturedImage(QImage image, int id);
    void updateTransformForItems();
    void updateItemsPosition();
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


#endif // CWVIEWPORTCAPTURE_H
