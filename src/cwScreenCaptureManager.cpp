/**************************************************************************
**
**    Copyright (C) 2014 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


//Our includes
#include "cwScreenCaptureManager.h"
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

cwScreenCaptureManager::cwScreenCaptureManager(QObject *parent) :
    QObject(parent),
    Resolution(300.0),
    BottomMargin(0.0),
    TopMargin(0.0),
    RightMargin(0.0),
    LeftMargin(0.0),
    NumberOfImagesProcessed(0),
    Columns(0),
    Rows(0),
    Scene(NULL)
{
}

/**
* @brief cwScreenCaptureManager::setView
* @param view
*/
void cwScreenCaptureManager::setView(cw3dRegionViewer* view) {
    if(View != view) {
        View = view;
        emit viewChanged();
    }
}

/**
* @brief cwScreenCaptureManager::setPaperSize
* @param paperSize
*/
void cwScreenCaptureManager::setPaperSize(QSizeF paperSize) {
    if(PaperSize != paperSize) {
        PaperSize = paperSize;
        emit paperSizeChanged();
    }
}

/**
* @brief cwScreenCaptureManager::setResolution
* @param resolution
*/
void cwScreenCaptureManager::setResolution(double resolution) {
    if(Resolution != resolution) {
        Resolution = resolution;
        emit resolutionChanged();
    }
}

/**
* @brief cwScreenCaptureManager::setBottomMargin
* @param bottomMargin
*/
void cwScreenCaptureManager::setBottomMargin(double bottomMargin) {
    if(BottomMargin != bottomMargin) {
        BottomMargin = bottomMargin;
        emit bottomMarginChanged();
    }
}

/**
* @brief cwScreenCaptureManager::setTopMargin
* @param topMargin
*/
void cwScreenCaptureManager::setTopMargin(double topMargin) {
    if(TopMargin != topMargin) {
        TopMargin = topMargin;
        emit topMarginChanged();
    }
}

/**
 * @brief cwScreenCaptureManager::setRightMargin
 * @param rightMargin
 */
void cwScreenCaptureManager::setRightMargin(double rightMargin) {
    if(RightMargin != rightMargin) {
        RightMargin = rightMargin;
        emit rightMarginChanged();
    }
}

/**
 * @brief cwScreenCaptureManager::setLeftMargin
 * @param leftMargin
 */
void cwScreenCaptureManager::setLeftMargin(double leftMargin) {
    if(LeftMargin != leftMargin) {
        LeftMargin = leftMargin;
        emit leftMarginChanged();
    }
}

/**
 * @brief cwScreenCaptureManager::setViewport
 * @param viewport
 *
 * This is the capturing viewport in opengl. This is the area that will be captured
 * and saved by the manager. The rectangle should be in pixels.
 */
void cwScreenCaptureManager::setViewport(QRect viewport) {
    if(Viewport != viewport) {
        Viewport = viewport;
        emit viewportChanged();
    }
}

/**
* @brief cwScreenCaptureManager::setFilename
* @param filename
*
* The filename of the output file
*/
void cwScreenCaptureManager::setFilename(QUrl filename) {
    if(Filename != filename) {
        Filename = filename;
        emit filenameChanged();
    }
}

/**
* @brief cwScreenCaptureManager::setFileType
* @param fileType
*/
void cwScreenCaptureManager::setFileType(FileType fileType) {
    if(Filetype != fileType) {
        Filetype = fileType;
        emit fileTypeChanged();
    }
}

/**
 * @brief cwScreenCaptureManager::capture
 *
 * Executes the screen capture
 */
void cwScreenCaptureManager::capture()
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

    QRect viewport = this->viewport(); //camera->viewport();

    qDebug() << "Viewport:" << viewport.width() << viewport.height();


    double pixelPerInchWidth = viewport.width() / paperSize().width();
    double pixelPerInchHeight = viewport.height() / paperSize().height();

    //Create the scale need to convert the viewport into the correct rendering resolution
    QPointF scale(resolution() / pixelPerInchWidth, resolution() / pixelPerInchHeight);

    QSize tileSize = QSize(1024, 1024); //viewport.size();
    QSize imageSize = QSize(viewport.width() * scale.x(), viewport.height() * scale.y());
    ImageSize = imageSize;
//    int border = 0;

//    int columns = (imageSize.width() + tileSize.width() - 1) / tileSize.width();
//    int rows = (imageSize.height() + tileSize.height() - 1) / tileSize.height();

    int columns = imageSize.width() / tileSize.width();
    int rows = imageSize.height() / tileSize.height();

    if(imageSize.width() % tileSize.width() > 0) { columns++; }
    if(imageSize.height() % tileSize.height() > 0) { rows++; }

    NumberOfImagesProcessed = 0;
    Columns = columns;
    Rows = rows;

    if(Scene != NULL) {
        Scene->deleteLater();
    }
    Scene = new QGraphicsScene(this);

    cwProjection croppedProjection = tileProjection(QRectF(QPointF(), viewport.size()),
                                                    camera->viewport().size(),
                                                    originalProj);

    for(int column = 0; column < columns; column++) {
        for(int row = 0; row < rows; row++) {

            int x = tileSize.width() * column;
            int y = tileSize.height() * row;

            QRect tileViewport(QPoint(x, y), tileSize);
            cwProjection tileProj = tileProjection(tileViewport, imageSize, croppedProjection);

            cwScreenCaptureCommand* command = new cwScreenCaptureCommand();

            cwCamera* croppedCamera = new cwCamera(command);
            croppedCamera->setViewport(QRect(QPoint(), tileSize));
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
 * @brief cwScreenCaptureManager::capturedImage
 * @param image
 */
void cwScreenCaptureManager::capturedImage(QImage image, int id)
{
    QSize tileSize = image.size();

    int row = Rows - (id / Columns);
    int column = id % Columns;

    double x = column * tileSize.width();
    double y = row * tileSize.height();

    cwGraphicsImageItem* graphicsImage = new cwGraphicsImageItem();
    graphicsImage->setImage(image);
    graphicsImage->setPos(QPointF(x, y));

    Scene->addItem(graphicsImage);

    //For debugging tiles
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
 * @brief cwScreenCaptureManager::exportScene
 */
void cwScreenCaptureManager::saveScene()
{
    QRectF sceneRect = Scene->sceneRect();
    sceneRect.setRight(sceneRect.x() + ImageSize.width());
    sceneRect.setTop(sceneRect.y() + (sceneRect.height() - ImageSize.height()));
    QImage outputImage(ImageSize, QImage::Format_ARGB32);
    outputImage.fill(Qt::transparent);

    cwImageResolution resolutionDPI(resolution(), cwUnits::DotsPerInch);
    cwImageResolution resolutionDPM = resolutionDPI.convertTo(cwUnits::DotsPerMeter);

    outputImage.setDotsPerMeterX(resolutionDPM.value());
    outputImage.setDotsPerMeterY(resolutionDPM.value());

    QPainter painter(&outputImage);
    Scene->render(&painter, QRectF(QPointF(), ImageSize), sceneRect);

    outputImage.save(filename().toLocalFile(), "png");

    Scene->deleteLater();
    Scene = NULL;

    emit finishedCapture();
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
cwProjection cwScreenCaptureManager::tileProjection(QRectF tileViewport,
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
