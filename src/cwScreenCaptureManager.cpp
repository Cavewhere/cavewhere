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

    QRect viewport = camera->viewport();

    double pixelPerInchWidth = viewport.width() / paperSize().width();
    double pixelPerInchHeight = viewport.height() / paperSize().height();

//    QSizeF screenDPI(pixelPerInchWidth, pixelPerInchHeight);
    QPointF scale(resolution() / pixelPerInchWidth, resolution() / pixelPerInchHeight);

    QSize tileSize = viewport.size();
    QSize imageSize = QSize(tileSize.width() * scale.x(), tileSize.height() * scale.y());
    int border = 0;

    int columns = (imageSize.width() + tileSize.width() - 1) / tileSize.width();
    int rows = (imageSize.height() + tileSize.height() - 1) / tileSize.height();

    NumberOfImagesProcessed = 0;
    Columns = columns;
    Rows = rows;

    if(Scene != NULL) {
        Scene->deleteLater();
    }
    Scene = new QGraphicsScene(this);

    for(int column = 0; column < columns; column++) {
        for(int row = 0; row < rows; row++) {

            double left = originalProj.left() + (originalProj.right() - originalProj.left())
                    * (column * tileSize.width() - border) / imageSize.width();
            double right = left + (originalProj.right() - originalProj.left()) * tileSize.width() / imageSize.width();
            double bottom = originalProj.bottom() + (originalProj.top() - originalProj.bottom())
                    * (row * tileSize.height() - border) / imageSize.height();
            double top = bottom + (originalProj.top() - originalProj.bottom()) * tileSize.height() / imageSize.height();

            cwProjection projection;
            switch(originalProj.type()) {
            case cwProjection::Perspective:
            case cwProjection::PerspectiveFrustum:
                projection.setFrustum(left, right, bottom, top, originalProj.near(), originalProj.far());
                break;
            case cwProjection::Ortho:
                projection.setOrtho(left, right, bottom, top, originalProj.near(), originalProj.far());
                break;
            default:
                break;
            }

            cwScreenCaptureCommand* command = new cwScreenCaptureCommand();

            cwCamera* croppedCamera = new cwCamera(command);
            croppedCamera->setViewport(QRect(QPoint(), viewport.size()));
            croppedCamera->setProjection(projection);
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
    QImage outputImage(sceneRect.size().toSize(), QImage::Format_ARGB32);
    QPainter painter(&outputImage);
    Scene->render(&painter);

    outputImage.save(filename().toLocalFile(), "png");

    Scene->deleteLater();
    Scene = NULL;

    emit finishedCapture();

//    QLabel* label = new QLabel();
//    label->setPixmap(QPixmap::fromImage(outputImage));
//    label->show();
}
