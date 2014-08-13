/**************************************************************************
**
**    Copyright (C) 2014 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


//Our includes
#include "cwViewportCapture.h"
#include "cwScale.h"
#include "cwScene.h"
#include "cwCamera.h"
#include "cw3dRegionViewer.h"
#include "cwScreenCaptureCommand.h"
#include "cwGraphicsImageItem.h"
#include "cwDebug.h"

cwViewportCapture::cwViewportCapture(QObject *parent) :
    QObject(parent),
    ScaleOrtho(new cwScale(this)),
    TileSize(1024, 1024),
    CaptureCamera(new cwCamera(this))
{
}

cwViewportCapture::~cwViewportCapture()
{
    delete PreviewItem;
    delete Item;
}

/**
* @brief cwScreenCaptureManager::setView
* @param view
*/
void cwViewportCapture::setView(cw3dRegionViewer* view) {
    if(View != view) {
        View = view;
        emit viewChanged();
    }
}

/**
* @brief cwViewportCapture::setResolution
* @param resolution
*/
void cwViewportCapture::setResolution(int resolution) {
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
void cwViewportCapture::setViewport(QRect viewport) {
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
 * @brief cwViewportCapture::capture
 */
void cwViewportCapture::capture()
{
    if(!viewport().size().isValid()) {
        qWarning() << "Viewport isn't valid for export:" << viewport();
        return;
    }

//    if(!paperSize().isValid()) {
//        qWarning() << "Paper size is invalid:" << paperSize();
//        return;
//    }

    cwScene* scene = view()->scene();
    cwCamera* camera = CaptureCamera;
    cwProjection originalProj = camera->projection();

    QRect viewport = this->viewport();
    QRect cameraViewport = camera->viewport();

    //Flip the viewport so it's in opengl coordinate system instead of qt
    viewport.moveTop(cameraViewport.height() - viewport.bottom());

    if(previewCapture()) {
        delete PreviewItem;
        PreviewItem = new QGraphicsItemGroup();
    } else {
        delete Item;
        Item = new QGraphicsItemGroup();
    }

    //Updates the scale for the preview item
    updateScalePreviewItem();

    //Calculate the scale
    double imageScale;
    if(previewCapture()) {
        imageScale = 1.0;
    } else {
        imageScale = PreviewItem->scale();
    }

    QSize tileSize = TileSize;
    QSize imageSize = QSize(viewport.width() * imageScale, viewport.height() * imageScale);

    int columns = imageSize.width() / tileSize.width();
    int rows = imageSize.height() / tileSize.height();

    if(imageSize.width() % tileSize.width() > 0) { columns++; }
    if(imageSize.height() % tileSize.height() > 0) { rows++; }

    NumberOfImagesProcessed = 0;
    Columns = columns;
    Rows = rows;

//    addBorderItem();

    cwProjection croppedProjection = tileProjection(viewport,
                                                    camera->viewport().size(),
                                                    originalProj);

    for(int column = 0; column < columns; column++) {
        for(int row = 0; row < rows; row++) {

            int x = tileSize.width() * column;
            int y = tileSize.height() * row;

            QSize croppedTileSize = calcCroppedTileSize(tileSize, imageSize, row, column);

            QRect tileViewport(QPoint(x, y), croppedTileSize);
            cwProjection tileProj = tileProjection(tileViewport, imageSize, croppedProjection);

            cwScreenCaptureCommand* command = new cwScreenCaptureCommand();

            cwCamera* croppedCamera = new cwCamera(command);
            croppedCamera->setViewport(QRect(QPoint(), croppedTileSize));
            croppedCamera->setProjection(tileProj);
            croppedCamera->setViewMatrix(camera->viewMatrix());

            command->setCamera(croppedCamera);
            command->setScene(scene);

            int id = row * columns + column;
            command->setId(id);

            connect(command, SIGNAL(createdImage(QImage,int)),
                    this, SLOT(capturedImage(QImage,int)),
                    Qt::QueuedConnection);
            scene->addSceneCommand(command);
        }
    }

    view()->update();
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
cwProjection cwViewportCapture::tileProjection(QRectF tileViewport,
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
QSize cwViewportCapture::calcCroppedTileSize(QSize tileSize, QSize imageSize, int row, int column) const
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
 * @brief cwScreenCaptureManager::capturedImage
 * @param image
 */
void cwViewportCapture::capturedImage(QImage image, int id)
{
    Q_UNUSED(id)
//    QSize tileSize = TileSize;

//    int row = Rows - (id / Columns) - 1;
//    int column = id % Columns;

//    double x = (viewport().x() * Scale) + column * tileSize.width();
//    double y = row * tileSize.height();

//    double bottom = (paperSize().height() - bottomMargin()) * resolution();
//    double yDiff = bottom - Rows * tileSize.height();
//    double yEdgeDiff = TileSize.height() - image.height();
//    y += yDiff + yEdgeDiff;

    QGraphicsItem* parent = previewCapture() ? PreviewItem : Item;

    cwGraphicsImageItem* graphicsImage = new cwGraphicsImageItem(parent);
    graphicsImage->setImage(image);
//    graphicsImage->setPos(QPointF(x, y));
}

/**
 * @brief cwViewportCapture::updateScalePreviewItem
 *
 * This updates the preview item's scale
 */
void cwViewportCapture::updateScalePreviewItem()
{

}

/**
* @brief cwScreenCaptureManager::view
* @return
*/
cw3dRegionViewer* cwViewportCapture::view() const {
    return View;
}
