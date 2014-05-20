/**************************************************************************
**
**    Copyright (C) 2014 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#ifndef CWSCREENCAPTUREMANAGER_H
#define CWSCREENCAPTUREMANAGER_H

//Qt includes
#include <QObject>
#include <QImage>
#include <QGraphicsScene>

//Our includes
#include "cw3dRegionViewer.h"

class cwScreenCaptureManager : public QObject
{
    Q_OBJECT

    Q_PROPERTY(cw3dRegionViewer* view READ view WRITE setView NOTIFY viewChanged)
    Q_PROPERTY(QSizeF paperSize READ paperSize WRITE setPaperSize NOTIFY paperSizeChanged)
    Q_PROPERTY(double resolution READ resolution WRITE setResolution NOTIFY resolutionChanged)
    Q_PROPERTY(double leftMargin READ leftMargin WRITE setLeftMargin NOTIFY leftMarginChanged)
    Q_PROPERTY(double rightMargin READ rightMargin WRITE setRightMargin NOTIFY rightMarginChanged)
    Q_PROPERTY(double topMargin READ topMargin WRITE setTopMargin NOTIFY topMarginChanged)
    Q_PROPERTY(double bottomMargin READ bottomMargin WRITE setBottomMargin NOTIFY bottomMarginChanged)
    Q_PROPERTY(QRect viewport READ viewport WRITE setViewport NOTIFY viewportChanged)
    Q_PROPERTY(QUrl filename READ filename WRITE setFilename NOTIFY filenameChanged)
    Q_PROPERTY(FileType fileType READ fileType WRITE setFileType NOTIFY fileTypeChanged)

    Q_ENUMS(FileType)
public:   
    enum FileType {
        PNG, // Raster export
        SVG, // Raster/Vector export
        PDF  // Raster/Vector export
    };

    explicit cwScreenCaptureManager(QObject *parent = 0);

    cw3dRegionViewer* view() const;
    void setView(cw3dRegionViewer* view);

    QSizeF paperSize() const;
    void setPaperSize(QSizeF paperSize);

    double resolution() const;
    void setResolution(double resolution);

    double bottomMargin() const;
    void setBottomMargin(double bottomMargin);

    double topMargin() const;
    void setTopMargin(double topMargin);

    double rightMargin() const;
    void setRightMargin(double rightMargin);

    double leftMargin() const;
    void setLeftMargin(double leftMargin);

    QRect viewport() const;
    void setViewport(QRect viewport);

    QUrl filename() const;
    void setFilename(QUrl filename);

    FileType fileType() const;
    void setFileType(FileType fileType);

    Q_INVOKABLE void capture();

signals:
    void viewChanged();
    void paperSizeChanged();
    void resolutionChanged();
    void bottomMarginChanged();
    void topMarginChanged();
    void rightMarginChanged();
    void leftMarginChanged();
    void viewportChanged();
    void filenameChanged();
    void fileTypeChanged();
    void finishedCapture();

public slots:

private slots:
    void capturedImage(QImage image, int id);

private:
    QPointer<cw3dRegionViewer> View; //!<
    QSizeF PaperSize; //!<
    double Resolution; //!<
    double BottomMargin; //!<
    double TopMargin; //!<
    double RightMargin; //!<
    double LeftMargin; //!<
    QRect Viewport; //!<
    QUrl Filename; //!<
    FileType Filetype; //!<

    //For processing
    int NumberOfImagesProcessed;
    int Columns;
    int Rows;
    QSize ImageSize;
    QGraphicsScene* Scene;

    void saveScene();

    cwProjection tileProjection(QRectF tileViewport,
                                QSizeF imageSize,
                                const cwProjection& originalProjection) const;


};

/**
* @brief cwScreenCaptureManager::view
* @return
*/
inline cw3dRegionViewer* cwScreenCaptureManager::view() const {
    return View;
}

/**
* @brief cwScreenCaptureManager::paperSize
* @return
*/
inline QSizeF cwScreenCaptureManager::paperSize() const {
    return PaperSize;
}

/**
* @brief cwScreenCaptureManager::resolution
* @return
*/
inline double cwScreenCaptureManager::resolution() const {
    return Resolution;
}

/**
* @brief cwScreenCaptureManager::bottomMargin
* @return
*/
inline double cwScreenCaptureManager::bottomMargin() const {
    return BottomMargin;
}

/**
* @brief cwScreenCaptureManager::topMargin
* @return
*/
inline double cwScreenCaptureManager::topMargin() const {
    return TopMargin;
}

/**
* @brief cwScreenCaptureManager::rightMargin
* @return
*/
inline double cwScreenCaptureManager::rightMargin() const {
    return RightMargin;
}

/**
* @brief cwScreenCaptureManager::leftMargin
* @return
*/
inline double cwScreenCaptureManager::leftMargin() const {
    return LeftMargin;
}

/**
* @brief cwScreenCaptureManager::viewport
* @return
*/
inline QRect cwScreenCaptureManager::viewport() const {
    return Viewport;
}

/**
* @brief cwScreenCaptureManager::filename
* @return
*/
inline QUrl cwScreenCaptureManager::filename() const {
    return Filename;
}

/**
* @brief cwScreenCaptureManager::fileType
* @return
*/
inline cwScreenCaptureManager::FileType cwScreenCaptureManager::fileType() const {
    return Filetype;
}

#endif // CWSCREENCAPTUREMANAGER_H
