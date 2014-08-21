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

//Our includes
class cwScale;
class cw3dRegionViewer;
class cwCamera;
#include "cwProjection.h"

class cwViewportCapture : public QObject
{
    Q_OBJECT

    Q_PROPERTY(int resolution READ resolution WRITE setResolution NOTIFY resolutionChanged)
    Q_PROPERTY(QRect viewport READ viewport WRITE setViewport NOTIFY viewportChanged)
    Q_PROPERTY(cwScale* scaleOrtho READ scaleOrtho CONSTANT) //For ortho projections
    Q_PROPERTY(cw3dRegionViewer* view READ view WRITE setView NOTIFY viewChanged)
    Q_PROPERTY(QGraphicsItem* previewItem READ previewItem NOTIFY previewItemChanged)
    Q_PROPERTY(QGraphicsItem* fullResolutionItem READ fullResolutionItem NOTIFY fullResolutionItemChanged)

public:
    explicit cwViewportCapture(QObject *parent = 0);
    virtual ~cwViewportCapture();

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
signals:
    void resolutionChanged();
    void viewportChanged();
    void viewChanged();
    void finishedCapture();
    void previewItemChanged();
    void fullResolutionItemChanged();

public slots:

private:
    //Properties
    QPointer<cw3dRegionViewer> View; //!<
    int Resolution; //!<
    cwScale* ScaleOrtho; //!<
    QRect Viewport;
    bool PreviewCapture;

    bool CapturingImages;
    int NumberOfImagesProcessed;
    int Columns;
    int Rows;
    QSize TileSize;

    //Scene state information
    cwCamera* CaptureCamera;

    QGraphicsItemGroup* PreviewItem; //This is the preview item
    QGraphicsItemGroup* Item; //This is the full resultion item

    void updateScalePreviewItem();
    cwProjection tileProjection(QRectF tileViewport,
                                QSizeF imageSize,
                                const cwProjection& originalProjection) const;

    QSize calcCroppedTileSize(QSize tileSize, QSize imageSize, int row, int column) const;

private slots:
    void capturedImage(QImage image, int id);
};





/**
* @brief cwViewportCapture::resolution
* @return
*/
inline int cwViewportCapture::resolution() const {
    return Resolution;
}

/**
* @brief class::scale
* @return
*/
inline cwScale* cwViewportCapture::scaleOrtho() const {
    return ScaleOrtho;
}

/**
* @brief cwScreenCaptureManager::viewport
* @return
*/
inline QRect cwViewportCapture::viewport() const {
    return Viewport;
}

/**
 * @brief previewCapture
 * @return
 */
inline bool cwViewportCapture::previewCapture() const {
    return PreviewCapture;
}

/**
 * @brief cwViewportCapture::setPreviewCapture
 * @param preview
 */
inline void cwViewportCapture::setPreviewCapture(bool preview)
{
    PreviewCapture = preview;
}

/**
* @brief cwViewportCapt::previewItem
* @return
*/
inline QGraphicsItem* cwViewportCapture::previewItem() const {
    return PreviewItem;
}

/**
* @brief cwViewportCapture::fullResolutionItem
* @return
*/
inline QGraphicsItem* cwViewportCapture::fullResolutionItem() const {
    return Item;
}

#endif // CWVIEWPORTCAPTURE_H
