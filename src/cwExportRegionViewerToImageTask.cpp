//Our includes
#include "cwExportRegionViewerToImageTask.h"

//Qt includes
#include <QLabel>
#include <QQuickWindow>
#include <QApplication>
#include <QScreen>
#include <QPainter>
#include <QTemporaryFile>

cwExportRegionViewerToImageTask::cwExportRegionViewerToImageTask(QObject *parent) :
    QObject(parent),
    Window(NULL),
    RegionViewer(NULL),
    DotPerInch(300)
{
}

/**
 * @brief cwExportRegionViewerToImageTask::setDPI
 * @param dotPerInch
 *
 * This will be the resulting image's dpi
 */
void cwExportRegionViewerToImageTask::setDPI(double dotPerInch)
{
    DotPerInch = dotPerInch;
}

void cwExportRegionViewerToImageTask::takeScreenshot() const
{
    if(Window == NULL || RegionViewer == NULL) {
        qDebug() << "Can't take screenshot because of a bug!" << LOCATION;
        return;
    }

    //Save old projection
    cwProjection originalProj = RegionViewer->camera()->projection();

    //Make sure the original projection is something we can work with
    if(originalProj.type() == cwProjection::Unknown) {
        qDebug() << "Don't know how to change projection, this is probably a bug" << LOCATION;
        return;
    }

    //Current resolution
    double currentResolution = QApplication::primaryScreen()->physicalDotsPerInch(); //DPI
    double resolutionScale = DotPerInch / currentResolution;

    QRectF bounds = RegionViewer->mapRectToScene(QRectF(0, 0, RegionViewer->width(), RegionViewer->height()));
    QSize tileSize = bounds.size().toSize();
    QSize imageSize = tileSize * resolutionScale;
    int border = 0;

    int columns = (imageSize.width() + tileSize.width() - 1) / tileSize.width();
    int rows = (imageSize.height() + tileSize.height() - 1) / tileSize.height();

    //Final image
    QImage combinedImage(imageSize, QImage::Format_ARGB32);
    QPainter painter(&combinedImage);

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

            RegionViewer->camera()->setProjection(projection);

            QImage grabbedWindow = Window->grabWindow();
            grabbedWindow = grabbedWindow.copy(bounds.toRect()); //Copy the image

            //Draw the tile on the final image
            double x = column * tileSize.width();
            double y = (imageSize.height() - row * tileSize.height()) - tileSize.height();
            QPoint position(x, y);
            painter.drawImage(position, grabbedWindow);


        }
    }

    painter.end();

    RegionViewer->camera()->setProjection(originalProj);

    QTemporaryFile tempFile;
    tempFile.setAutoRemove(false);
    tempFile.open();

    QString filename = tempFile.fileName() + ".png";
    combinedImage.save(filename);
    qDebug() << "Saved screenshot to:" << filename;
}

/**
Sets window
*/
void cwExportRegionViewerToImageTask::setWindow(QQuickWindow* window) {
    if(Window != window) {
        Window = window;
        emit windowChanged();
    }
}

/**
Sets regionViewer
*/
void cwExportRegionViewerToImageTask::setRegionViewer(cw3dRegionViewer* regionViewer) {
    if(RegionViewer != regionViewer) {
        RegionViewer = regionViewer;
        emit regionViewerChanged();
    }
}

