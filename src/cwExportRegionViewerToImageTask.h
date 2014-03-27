/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWEXPORTSCENEIMAGETASK_H
#define CWEXPORTSCENEIMAGETASK_H

//Our includes
#include "cwTask.h"
#include "cw3dRegionViewer.h"
#include "cwDebug.h"

//Qt includes
#include <QImage>
#include <QQuickView>

/**
 * @brief The cwExportRegionViewerToImageTask class
 *
 * This exports the cw3dRegionViewer to an image.  This will use the current camera and
 */
class cwSceneToImageTask : public QObject
{
    Q_OBJECT

//    Q_PROPERTY(QQuickView* window READ window WRITE setWindow NOTIFY windowChanged)
//    Q_PROPERTY(cw3dRegionViewer* regionViewer READ regionViewer WRITE setRegionViewer NOTIFY regionViewerChanged)

    Q_PROPERTY(int dpi READ dpi WRITE setDPI NOTIFY dpiChanged)
    Q_PROPERTY(double leftMargin READ leftMargin WRITE setLeftMargin NOTIFY leftMarginChanged)
    Q_PROPERTY(double rightMargin READ rightMargin WRITE setRightMargin NOTIFY rightMarginChanged)
    Q_PROPERTY(double topMargin READ topMargin WRITE setTopMargin NOTIFY topMarginChanged)
    Q_PROPERTY(double bottomMargin READ bottomMargin WRITE setBottomMargin NOTIFY bottomMarginChanged)
    Q_PROPERTY(QSize paperSize READ paperSize WRITE setPaperSize NOTIFY paperSizeChanged)
    Q_PROPERTY(Orienation orienation READ orienation WRITE setOrienation NOTIFY orienationChanged)
    Q_PROPERTY(QString paperUnits READ paperUnits WRITE setPaperUnits NOTIFY paperUnitsChanged)

    Q_ENUMS(Orienation)
public:
    enum Orienation {
        Portrait,
        Landscape
    };

    cwSceneToImageTask(QObject* parent = NULL);

//    QQuickView* window() const;
//    void setWindow(QQuickView* window);

    //Inputs
    int dpi() const;
    void setDPI(int dpi);

    double leftMargin() const;
    void setLeftMargin(double leftMargin);

    double rightMargin() const;
    void setRightMargin(double rightMargin);

    double topMargin() const;
    void setTopMargin(double topMargin);

    double bottomMargin() const;
    void setBottomMargin(double bottomMargin);

    QSize paperSize() const;
    void setPaperSize(QSize paperSize);

    QString paperUnits() const;
    void setPaperUnits(QString paperUnits);

    Orienation orienation() const;
    void setOrienation(Orienation orienation);

public slots:
    void takeScreenshot() const;

signals:
    void dpiChanged();

    void leftMarginChanged();
    void rightMarginChanged();
    void topMarginChanged();
    void bottomMarginChanged();
    void paperSizeChanged();
    void orienationChanged();
    void paperUnitsChanged();
//    void windowChanged();
//    void renderingBoundsChanged();
//    void regionViewerChanged();

protected:

private:
    QQuickView* Window; //!<
    cw3dRegionViewer* RegionViewer; //!<

    //Inputs
    int DPI;  //Dots per inch

    //Margins
    double LeftMargin; //!<
    double RightMargin; //!<
    double TopMargin; //!<
    double BottomMargin; //!<

    QSize PaperSize; //!<
    QString PaperUnits; //!<

    Orienation PaperOrienation; //!<
};

/**
* @brief cwExportRegionViewerToImageTask::dpi
* @return
*/
inline int cwSceneToImageTask::dpi() const {
    return DPI;
}

/**
* @brief cwSceneToImageTask::leftMargin
* @return
*/
inline double cwSceneToImageTask::leftMargin() const {
    return LeftMargin;
}


/**
* @brief cwSceneToImageTask::rightMargin
* @return
*/
inline double cwSceneToImageTask::rightMargin() const {
    return RightMargin;
}

/**
* @brief cwSceneToImageTask::topMargin
* @return
*/
inline double cwSceneToImageTask::topMargin() const {
    return TopMargin;
}

/**
* @brief cwSceneToImageTask::bottomMarign
* @return
*/
inline double cwSceneToImageTask::bottomMargin() const {
    return BottomMargin;
}

/**
* @brief cwSceneToImageTask::paperSize
* @return
*/
inline QSize cwSceneToImageTask::paperSize() const {
    return PaperSize;
}

/**
* @brief cwSceneToImageTask::orienation
* @return
*/
inline cwSceneToImageTask::Orienation cwSceneToImageTask::orienation() const {
    return PaperOrienation;
}

/**
* @brief cwSceneToImageTask::paperUnits
* @return
*/
inline QString cwSceneToImageTask::paperUnits() const {
    return PaperUnits;
}


///**
//Gets window
//*/
//inline QQuickView* cwSceneToImageTask::window() const {
//    return Window;
//}

///**
//Gets regionViewer
//*/
//inline cw3dRegionViewer* cwSceneToImageTask::regionViewer() const {
//    return RegionViewer;
//}

#endif // CWEXPORTSCENEIMAGETASK_H
