//Our includes
#include "cwExportRegionViewerToImageTask.h"

//Qt includes
#include <QLabel>
#include <QQuickWindow>

cwExportRegionViewerToImageTask::cwExportRegionViewerToImageTask() :
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
void cwExportRegionViewerToImageTask::setDPI(int dotPerInch)
{
    DotPerInch = dotPerInch;
}

void cwExportRegionViewerToImageTask::takeScreenshot() const
{
    if(Window == NULL || RegionViewer == NULL) {
        qDebug() << "Can't take screenshot because of a bug!" << LOCATION;
        return;
    }

    for(int i = 1; i <= 5; i++) {
        QMatrix4x4 matrix = RegionViewer->camera()->viewMatrix();
        matrix.translate(0, 0, -100 * i);
        RegionViewer->camera()->setViewMatrix(matrix);

        QRectF bounds = RegionViewer->mapRectToScene(QRectF(0, 0, RegionViewer->width(), RegionViewer->height()));

        qDebug() << "Taking screeshot " << bounds;
        QImage grabbedWindow = Window->grabWindow();
        grabbedWindow = grabbedWindow.copy(bounds.toRect());
        QLabel* label = new QLabel();
        label->setPixmap(QPixmap::fromImage(grabbedWindow));
        label->show();
    }
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

