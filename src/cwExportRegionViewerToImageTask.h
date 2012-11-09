#ifndef CWEXPORTSCENEIMAGETASK_H
#define CWEXPORTSCENEIMAGETASK_H

//Our includes
#include "cwTask.h"
#include "cw3dRegionViewer.h"
#include "cwDebug.h"

//Qt includes
#include <QImage>
#include <QQuickWindow>

/**
 * @brief The cwExportRegionViewerToImageTask class
 *
 * This exports the cw3dRegionViewer to an image.  This will use the current camera and
 */
class cwExportRegionViewerToImageTask : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QQuickWindow* window READ window WRITE setWindow NOTIFY windowChanged)
    Q_PROPERTY(cw3dRegionViewer* regionViewer READ regionViewer WRITE setRegionViewer NOTIFY regionViewerChanged)

public:
    cwExportRegionViewerToImageTask(QObject* parent = NULL);

    QQuickWindow* window() const;
    void setWindow(QQuickWindow* window);

    //Inputs
    void setDPI(double dotPerInch);

    cw3dRegionViewer* regionViewer() const;
    void setRegionViewer(cw3dRegionViewer* regionViewer);

public slots:
    void takeScreenshot() const;

signals:
    void windowChanged();
    void renderingBoundsChanged();
    void regionViewerChanged();

protected:

private:
    QQuickWindow* Window; //!<
    cw3dRegionViewer* RegionViewer; //!<

    //Inputs
    double DotPerInch;

};

/**
Gets window
*/
inline QQuickWindow* cwExportRegionViewerToImageTask::window() const {
    return Window;
}

/**
Gets regionViewer
*/
inline cw3dRegionViewer* cwExportRegionViewerToImageTask::regionViewer() const {
    return RegionViewer;
}

#endif // CWEXPORTSCENEIMAGETASK_H
