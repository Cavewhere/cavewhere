/**************************************************************************
**
**    Copyright (C) 2014 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


//Our includes
#include "cwCaptureManager.h"
#include "cw3dRegionViewer.h"
#include "cwScreenCaptureCommand.h"
#include "cwScene.h"
#include "cwGraphicsImageItem.h"
#include "cwDebug.h"
#include "cwImageResolution.h"

//Qt includes
#include <QLabel>
#include <QPixmap>
#include <QScopedPointer>
#include <QGraphicsScene>
#include <QPainter>

cwCaptureManager::cwCaptureManager(QObject *parent) :
    QObject(parent),
    Resolution(300.0),
    BottomMargin(0.0),
    TopMargin(0.0),
    RightMargin(0.0),
    LeftMargin(0.0),
    NumberOfImagesProcessed(0),
    Columns(0),
    Rows(0),
    TileSize(1024, 1024),
    Scene(new QGraphicsScene(this)),
    Scale(1.0)
{
    Scene->addRect(QRectF(10, 10, 100, 100));
}

/**
* @brief cwCaptureManager::setView
* @param view
*/
void cwCaptureManager::setView(cw3dRegionViewer* view) {
    if(View != view) {
        View = view;
        emit viewChanged();
    }
}

/**
* @brief cwCaptureManager::setPaperSize
* @param paperSize
*/
void cwCaptureManager::setPaperSize(QSizeF paperSize) {
    if(PaperSize != paperSize) {
        PaperSize = paperSize;
        emit paperSizeChanged();
    }
}

/**
* @brief cwCaptureManager::setResolution
* @param resolution
*/
void cwCaptureManager::setResolution(double resolution) {
    if(Resolution != resolution) {
        Resolution = resolution;
        emit resolutionChanged();
    }
}

/**
* @brief cwCaptureManager::setBottomMargin
* @param bottomMargin
*/
void cwCaptureManager::setBottomMargin(double bottomMargin) {
    if(BottomMargin != bottomMargin) {
        BottomMargin = bottomMargin;
        emit bottomMarginChanged();
    }
}

/**
* @brief cwCaptureManager::setTopMargin
* @param topMargin
*/
void cwCaptureManager::setTopMargin(double topMargin) {
    if(TopMargin != topMargin) {
        TopMargin = topMargin;
        emit topMarginChanged();
    }
}

/**
 * @brief cwCaptureManager::setRightMargin
 * @param rightMargin
 */
void cwCaptureManager::setRightMargin(double rightMargin) {
    if(RightMargin != rightMargin) {
        RightMargin = rightMargin;
        emit rightMarginChanged();
    }
}

/**
 * @brief cwCaptureManager::setLeftMargin
 * @param leftMargin
 */
void cwCaptureManager::setLeftMargin(double leftMargin) {
    if(LeftMargin != leftMargin) {
        LeftMargin = leftMargin;
        emit leftMarginChanged();
    }
}

/**
 * @brief cwCaptureManager::setViewport
 * @param viewport
 *
 * This is the capturing viewport in opengl. This is the area that will be captured
 * and saved by the manager. The rectangle should be in pixels.
 */
void cwCaptureManager::setViewport(QRect viewport) {
    if(Viewport != viewport) {
        Viewport = viewport;
        emit viewportChanged();
    }
}

/**
* @brief cwCaptureManager::setFilename
* @param filename
*
* The filename of the output file
*/
void cwCaptureManager::setFilename(QUrl filename) {
    if(Filename != filename) {
        Filename = filename;
        emit filenameChanged();
    }
}

/**
* @brief cwCaptureManager::setFileType
* @param fileType
*/
void cwCaptureManager::setFileType(FileType fileType) {
    if(Filetype != fileType) {
        Filetype = fileType;
        emit fileTypeChanged();
    }
}

/**
 * @brief cwCaptureManager::capture
 *
 * Executes the screen capture
 */
void cwCaptureManager::capture()
{
    if(!viewport().size().isValid()) {
        qWarning() << "Viewport isn't valid for export:" << viewport();
        return;
    }

    if(!paperSize().isValid()) {
        qWarning() << "Paper size is invalid:" << paperSize();
        return;
    }

    cwScene* scene = view()->scene();
    cwCamera* camera = view()->camera();
    cwProjection originalProj = camera->projection();

    QRect viewport = this->viewport();
    QRect cameraViewport = camera->viewport();

    //Flip the viewport so it's in opengl coordinate system instead of qt
    viewport.moveTop(cameraViewport.height() - viewport.bottom());

    calcScale();

    QSize tileSize = TileSize;
    QSize imageSize = QSize(viewport.width() * Scale, viewport.height() * Scale);

    int columns = imageSize.width() / tileSize.width();
    int rows = imageSize.height() / tileSize.height();

    if(imageSize.width() % tileSize.width() > 0) { columns++; }
    if(imageSize.height() % tileSize.height() > 0) { rows++; }

    NumberOfImagesProcessed = 0;
    Columns = columns;
    Rows = rows;

    Scene->clear();

    addBorderItem();

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
 * @brief cwCaptureManager::addViewportCapture
 * @param capture
 *
 * This adds the viewport capture to the capture manager
 */
void cwCaptureManager::addViewportCapture(cwViewportCapture *capture)
{
    if(Captures.contains(capture)) {
        qWarning() << "Capture already added:" << capture << LOCATION;
        return;
    }

    if(capture == nullptr) { return; }
    if(capture->previewItem() != nullptr) {
        Scene->addItem(capture->previewItem());
    }
    if(capture->fullResolutionItem() != nullptr) {
        Scene->addItem(capture->fullResolutionItem());
    }

    connect(capture, &cwViewportCapture::previewItemChanged, this, &cwCaptureManager::addPreviewCaptureItem);
    connect(capture, &cwViewportCapture::fullResolutionItemChanged, this, &cwCaptureManager::addFullResultionCaptureItem);

    Captures.insert(capture);
    emit numberOfCapturesChanged();
}

void cwCaptureManager::removeViewportCapture(cwViewportCapture *capture)
{
    Q_UNUSED(capture)

}

/**
 * @brief cwCaptureManager::capturedImage
 * @param image
 */
void cwCaptureManager::capturedImage(QImage image, int id)
{
    QSize tileSize = TileSize;

    int row = Rows - (id / Columns) - 1;
    int column = id % Columns;

    double x = (viewport().x() * Scale) + column * tileSize.width();
    double y = row * tileSize.height();

    double bottom = (paperSize().height() - bottomMargin()) * resolution();
    double yDiff = bottom - Rows * tileSize.height();
    double yEdgeDiff = TileSize.height() - image.height();
    y += yDiff + yEdgeDiff;

    cwGraphicsImageItem* graphicsImage = new cwGraphicsImageItem();
    graphicsImage->setImage(image);
    graphicsImage->setPos(QPointF(x, y));

    Scene->addItem(graphicsImage);

//    //For debugging tiles
//    QRectF tileRect = QRectF(QPointF(x, y), image.size());
//    Scene->addRect(tileRect, QPen(Qt::red));
//    QGraphicsSimpleTextItem* textItem = Scene->addSimpleText(QString("Id:%1").arg(id));
//    textItem->setPen(QPen(Qt::red));
//    textItem->setPos(tileRect.center());

    NumberOfImagesProcessed++;
    if(NumberOfImagesProcessed == Columns * Rows) {
        //We've added all the tiles to the scene, now we're ready to
        //save the scene
        saveScene();
    }
}

/**
 * @brief cwCaptureManager::addPreviewCaptureItem
 *
 * This will add the preview item to the graphics scene of the preview item
 *
 * This function should only be called through a signal and slot
 */
void cwCaptureManager::addPreviewCaptureItem()
{
    Q_ASSERT(dynamic_cast<cwViewportCapture*>(sender()) != nullptr);
    cwViewportCapture* viewportCapture = static_cast<cwViewportCapture*>(sender());

    if(viewportCapture->previewItem() != nullptr) {
        Scene->addItem(viewportCapture->previewItem());
    }
}

/**
 * @brief cwCaptureManager::addPreviewCaptureItem
 *
 * This will add the full item to the graphics scene of the preview item
 *
 * This function should only be called through a signal and slot
 */
void cwCaptureManager::addFullResultionCaptureItem()
{
    Q_ASSERT(dynamic_cast<cwViewportCapture*>(sender()) != nullptr);
    cwViewportCapture* viewportCapture = static_cast<cwViewportCapture*>(sender());

    if(viewportCapture->previewItem() != nullptr) {
        Scene->addItem(viewportCapture->previewItem());
    }
}

/**
 * @brief cwCaptureManager::exportScene
 */
void cwCaptureManager::saveScene()
{
    QSizeF imageSize = paperSize() * resolution();

    QRectF sceneRect = QRectF(QPointF(), imageSize); //Scene->itemsBoundingRect();
    QImage outputImage(imageSize.toSize(), QImage::Format_ARGB32);
    outputImage.fill(Qt::white); //transparent);

    cwImageResolution resolutionDPI(resolution(), cwUnits::DotsPerInch);
    cwImageResolution resolutionDPM = resolutionDPI.convertTo(cwUnits::DotsPerMeter);

    outputImage.setDotsPerMeterX(resolutionDPM.value());
    outputImage.setDotsPerMeterY(resolutionDPM.value());

    QPainter painter(&outputImage);
    Scene->render(&painter, sceneRect, sceneRect);

    outputImage.save(filename().toLocalFile(), "png");

    Scene->clear();

    emit finishedCapture();
}

/**
 * @brief cwCaptureManager::tileProjection
 * @param viewport
 * @param originalProjection
 * @return
 *
 * This will take original viewport and original projection and find the
 * tile projection matrix using the tileViewport. The tileViewport
 * should be a sub rectangle of the original viewport. This function
 * will work with orthognal and perspective projections
 */
cwProjection cwCaptureManager::tileProjection(QRectF tileViewport,
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
 * @brief cwCaptureManager::addBorderItem
 *
 * This function will add a new border item to the scene
 */
void cwCaptureManager::addBorderItem()
{
    QPen pen;
    pen.setWidthF(4.0);
    pen.setColor(Qt::black);

    QGraphicsRectItem* rectItem = new QGraphicsRectItem();
    rectItem->setPen(QPen());
    rectItem->setBrush(Qt::NoBrush);
    rectItem->setRect(viewport());
    rectItem->setScale(Scale);
    rectItem->setZValue(1.0);

    Scene->addItem(rectItem);
}

/**
 * @brief cwCaptureManager::calcScale
 *
 * This caluclates the scale
 */
void cwCaptureManager::calcScale()
{
    double pixelPerInchWidth = screenPaperSize().width() / paperSize().width();
    double pixelPerInchHeight = screenPaperSize().height() / paperSize().height();

    qDebug() << "Pixel per inch: " << pixelPerInchHeight << pixelPerInchWidth;

    //Create the scale need to convert the viewport into the correct rendering resolution
    QPointF scale(resolution() / pixelPerInchWidth, resolution() / pixelPerInchHeight);

    if(scale.x() != scale.y()) {
        qWarning() << "Scale isn't equal in x " << scale.x() << "y" << scale.y() << "using X" << LOCATION;
    }

    Scale = scale.x();
}

/**
 * @brief cwCaptureManager::croppedTile
 * @param tileSize
 * @param imageSize
 * @param row
 * @param column
 * @return Return the tile size of row and column
 *
 * This may crop the tile, if the it goes beyond the imageSize
 */
QSize cwCaptureManager::calcCroppedTileSize(QSize tileSize, QSize imageSize, int row, int column) const
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
* @brief cwCaptureManager::setScreenPaperSize
* @param screenPaperSize - The paperSize in pixel, this is what shown on the screen
*/
void cwCaptureManager::setScreenPaperSize(QSizeF screenPaperSize) {
    if(ScreenPaperSize != screenPaperSize) {
        ScreenPaperSize = screenPaperSize;
        emit screenPaperSizeChanged();
    }
}
