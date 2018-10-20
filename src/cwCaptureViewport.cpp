/**************************************************************************
**
**    Copyright (C) 2014 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Qt includes
#include <QPen>

//Our includes
#include "cwCaptureViewport.h"
#include "cwScale.h"
#include "cwScene.h"
#include "cwCamera.h"
#include "cwScreenCaptureCommand.h"
#include "cwGraphicsImageItem.h"
#include "cwDebug.h"
#include "cwLength.h"
#include "cwGlobals.h"

//undef these because microsoft is fucking retarded...
#ifdef Q_OS_WIN
#undef far
#undef near
#endif

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
    CaptureCamera(new cwCamera()),
    PreviewItem(new QGraphicsItemGroup()),
    Item(new QGraphicsItemGroup())
{
//    CaptureCamera->setParent(this);

    connect(ScaleOrtho, &cwScale::scaleChanged, this, &cwCaptureViewport::updateTransformForItems);
    connect(this, &cwCaptureViewport::positionOnPaperChanged, this, &cwCaptureViewport::updateItemsPosition);
    connect(this, &cwCaptureViewport::rotationChanged, this, &cwCaptureViewport::updateTransformForItems);
}

cwCaptureViewport::~cwCaptureViewport()
{
    deleteSceneItems();
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

        if(!Camera.isNull()) {
            CaptureCamera->setProjection(Camera->projection());
            CaptureCamera->setViewport(Camera->viewport());
            CaptureCamera->setViewMatrix(Camera->viewMatrix());
        }
    }
}

/**
 * @brief cwCaptureViewport::capture
 */
void cwCaptureViewport::capture()
{
    if(CapturingImages) { return; }
    CapturingImages = true;

//    Commands.clear(); //Clear previous commands

    if(!viewport().size().isValid()) {
        qWarning() << "Viewport isn't valid for export:" << viewport();
        return;
    }

    //Delete previous capture images
    clearSceneItemChildren();

//    cwScene* scene = view()->scene();
    cwCamera* camera = CaptureCamera;
    cwProjection originalProj = camera->projection();

    QRect viewport = this->viewport();
    QRect cameraViewport = camera->viewport();

    //Flip the viewport so it's in opengl coordinate system instead of qt
    viewport.moveTop(cameraViewport.height() - viewport.bottom());

    double imageScale;
    if(previewCapture()) {
        imageScale = 1.0;
    } else {
        imageScale = ItemScale * resolution();
    }

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

//    IdToOrigin.clear(); //This is used to keep track the positions of the images

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

            auto command = new cwScreenCaptureCommand();
//            Commands.append(command);

            cwCamera* croppedCamera = new cwCamera();
//            croppedCamera->setParent(command);
            croppedCamera->setViewport(QRect(QPoint(), croppedTileSize));
            croppedCamera->setProjection(tileProj);
            croppedCamera->setViewMatrix(camera->viewMatrix());

            qDebug() << "column:" << column << "row:" << row << "Projection:" << tileProj.matrix();
            qDebug() << "column:" << column << "row:" << row << "View:" << croppedCamera->viewMatrix();

            command->setCamera(croppedCamera);
//            command->setScene(scene);

//            int id = row * columns + column;
//            Q_ASSERT(Commands.size() - 1 == id);
//            Q_ASSERT(Commands.last() == command);
//            command->setId(id);

            double originX = column * tileSize.width();
            double originY = onPaperViewport.height() - (row * tileSize.height() + croppedTileSize.height());
            QPointF origin(originX, originY);

            command->setOrigin(origin);
//            IdToOrigin[id] = origin;

            connect(command, &cwScreenCaptureCommand::imageChanged,
                    this,  &cwCaptureViewport::capturedImage);

            emit screenCaptureRequested(command);
        }
    }

//    view()->update();
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
    }

    if(Item != nullptr) {
        delete Item;
        Item = nullptr;
    }
}

/**
 * @brief cwCaptureViewport::clearSceneItemChildren
 *
 * This function deletes all children of a scene item. This will
 * remove all image data that's associated with a capture.
 */
void cwCaptureViewport::clearSceneItemChildren()
{
    auto parent = previewCapture() ? PreviewItem : Item;
    for(auto child : parent->childItems()) {
        delete child;
    }
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
        double tilesInImageX = imageSize.width() / static_cast<double>(tileSize.width());
        double extraX = tilesInImageX - floor(tilesInImageX);
        double imageWidthCrop = ceil(tileSize.width() * extraX);
        croppedTileSize.setWidth(imageWidthCrop);
    }

    if(row == lastRowIndex && !exactEdgeY)
    {
        //Edge tile, crop it
        double tilesInImageY = imageSize.height() / static_cast<double>(tileSize.height());
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
    QRectF paperRect = previewItem()->boundingRect();
    QRectF boundingBoxRect = transform.mapRect(paperRect);
    setBoundingBox(boundingBoxRect);
}

/**
 * @brief cwScreenCaptureManager::capturedImage
 *
 * This function assumes that the function was called through a signal and slot. From
 * a valid cwScreenCatpureCommand.
 */
void cwCaptureViewport::capturedImage()
{
    Q_ASSERT(CapturingImages); //Most be in the capture images state

    Q_ASSERT(dynamic_cast<cwScreenCaptureCommand*>(sender()) != nullptr);
    cwScreenCaptureCommand* command = static_cast<cwScreenCaptureCommand*>(sender());

    QPointF origin = command->origin(); //IdToOrigin.value(id);

    QGraphicsItemGroup* parent = previewCapture() ? PreviewItem : Item;

    cwGraphicsImageItem* graphicsImage = new cwGraphicsImageItem(parent);
    graphicsImage->setImage(command->image());
    graphicsImage->setPos(origin);
    parent->addToGroup(graphicsImage);

    command->deleteLater();

    //For debugging tiles
    QRectF tileRect = QRectF(origin, command->image().size());
    QGraphicsRectItem* rectItem = new QGraphicsRectItem(parent);
    rectItem->setPen(QPen(Qt::red));
    rectItem->setRect(tileRect);
    QGraphicsSimpleTextItem* textItem = new QGraphicsSimpleTextItem(parent);
//    textItem->setText(QString("Id:%1").arg(id));
    textItem->setPen(QPen(Qt::red));
    textItem->setPos(tileRect.center());

    NumberOfImagesProcessed++;

    if(NumberOfImagesProcessed == Rows * Columns) {
        //Finished capturing images
        NumberOfImagesProcessed = 0;
        Rows = 0;
        Columns = 0;
        CapturingImages = false;

        if(previewCapture()) {
            updateBoundingBox();
        }

        emit finishedCapture();
    }
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

}

///**
//* @brief cwScreenCaptureManager::view
//* @return
//*/
//cw3dRegionViewer* cwCaptureViewport::view() const {
//    return View;
//}

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

/**
* @brief cwCaptureViewport::camera
*/
void cwCaptureViewport::setCamera(cwCamera* camera) {
    if(Camera != camera) {
        Camera = camera;
        emit cameraChanged();
    }
}

/**
* @brief cwScreenCaptureCommand::camera
* @return The view's camera
*/
cwCamera* cwCaptureViewport::camera() const {
    return Camera;
}

