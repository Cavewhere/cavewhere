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
#include <QAbstractListModel>
#include <QPointer>
#include <QUrl>

//Our includes
#include "cw3dRegionViewer.h"
class cwCaptureViewport;
class cwCaptureItem;
#include "cwProjection.h"
class cwCaptureGroupModel;

class cwCaptureManager : public QAbstractListModel
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
    Q_PROPERTY(QStringList fileTypes READ fileTypes CONSTANT)

    Q_PROPERTY(QGraphicsScene* scene READ scene CONSTANT)
    Q_PROPERTY(int numberOfCaptures READ numberOfCaptures NOTIFY numberOfCapturesChanged)
    Q_PROPERTY(cwCaptureGroupModel* groupModel READ groupModel CONSTANT)

    Q_PROPERTY(double memoryRequired READ memoryRequired NOTIFY memoryRequiredChanged)
    Q_PROPERTY(double memoryLimit READ memoryLimit CONSTANT)

    Q_ENUMS(FileType Roles)
public:   
    enum FileType {
        UnknownType,
        PNG, // Raster export
        TIF, // Raster export
        JPG, // Raster export
        SVG, // Raster/Vector export
        PDF // Raster/Vector export
    };

    enum Roles {
        LayerObjectRole,
        LayerNameRole
    };

    explicit cwCaptureManager(QObject *parent = 0);
    virtual ~cwCaptureManager();

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

    cwCaptureGroupModel* groupModel() const;

    Q_INVOKABLE void capture();

    QGraphicsScene* scene() const;

    Q_INVOKABLE void addCaptureViewport(cwCaptureViewport* capture);
    Q_INVOKABLE void removeCaptureViewport(cwCaptureViewport* capture);

    int numberOfCaptures() const;

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    Q_INVOKABLE QVariant data(const QModelIndex &index, int role) const;
    Q_INVOKABLE QModelIndex index(int row, int column = 0, const QModelIndex& parent = QModelIndex()) const;

    QHash<int, QByteArray> roleNames() const;

    QStringList fileTypes() const;
    Q_INVOKABLE FileType typeNameToFileType(QString fileType) const;

    double memoryRequired() const;
    double memoryLimit() const;

signals:
    void viewChanged();
    void paperSizeChanged();
    void screenPaperSizeChanged();
    void resolutionChanged();
    void bottomMarginChanged();
    void topMarginChanged();
    void rightMarginChanged();
    void leftMarginChanged();
    void viewportChanged();
    void filenameChanged();
    void fileTypeChanged();
    void finishedCapture();
    void numberOfCapturesChanged();
    void aboutToDestoryManager();
    void memoryRequiredChanged();

public slots:

private slots:
//    void capturedImage(QImage image, int id);

    void addPreviewCaptureItem();
    void addFullResultionCaptureItem();

    void saveCaptures();

private:
    QPointer<cw3dRegionViewer> View; //!<
    QSizeF PaperSize; //!<
    QSizeF ScreenPaperSize; //!<
    double Resolution; //!<
    double BottomMargin; //!<
    double TopMargin; //!<
    double RightMargin; //!<
    double LeftMargin; //!<
    QRect Viewport; //!<
    QUrl Filename; //!<
    FileType Filetype; //!<
    cwCaptureGroupModel* GroupModel; //!<
    const QMap<QString, FileType> FileTypes = {
        {"PNG", PNG},
        {"TIF", TIF},
        {"JPG", JPG},
        {"SVG", SVG},
        {"PDF", PDF}
    };

    //For internal drawing

    //For processing
    int NumberOfImagesProcessed;
    QGraphicsScene* Scene;
    QGraphicsRectItem* PaperRectangle;
    QGraphicsRectItem* BorderRectangle;
    double Scale;

    QList<cwCaptureViewport*> Captures;
    QList<cwCaptureItem*> Layers;

    void saveScene();

    cwProjection tileProjection(QRectF tileViewport,
                                QSizeF imageSize,
                                const cwProjection& originalProjection) const;


    void addBorderItem();
    QSize calcCroppedTileSize(QSize tileSize, QSize imageSize, int row, int column) const;

    void scaleCaptureToFitPage(cwCaptureViewport* item);

    void addPreviewCaptureItemHelper(cwCaptureViewport* capture);
    void addFullResultionCaptureItemHelper(cwCaptureViewport* capture);

    void updateBorderRectangle();

    qint64 requiredSizeInBytes() const;

};

/**
* @brief cwScreenCaptureManager::view
* @return
*/
inline cw3dRegionViewer* cwCaptureManager::view() const {
    return View;
}

/**
* @brief cwScreenCaptureManager::paperSize in paper units (in or mm)
* @return
*/
inline QSizeF cwCaptureManager::paperSize() const {
    return PaperSize;
}


/**
* @brief cwScreenCaptureManager::resolution
* @return
*/
inline double cwCaptureManager::resolution() const {
    return Resolution;
}

/**
* @brief cwScreenCaptureManager::bottomMargin
* @return
*/
inline double cwCaptureManager::bottomMargin() const {
    return BottomMargin;
}

/**
* @brief cwScreenCaptureManager::topMargin
* @return
*/
inline double cwCaptureManager::topMargin() const {
    return TopMargin;
}

/**
* @brief cwScreenCaptureManager::rightMargin
* @return
*/
inline double cwCaptureManager::rightMargin() const {
    return RightMargin;
}

/**
* @brief cwScreenCaptureManager::leftMargin
* @return
*/
inline double cwCaptureManager::leftMargin() const {
    return LeftMargin;
}

/**
* @brief cwScreenCaptureManager::viewport
* @return
*/
inline QRect cwCaptureManager::viewport() const {
    return Viewport;
}

/**
* @brief cwScreenCaptureManager::filename
* @return
*/
inline QUrl cwCaptureManager::filename() const {
    return Filename;
}

/**
* @brief cwScreenCaptureManager::fileType
* @return
*/
inline cwCaptureManager::FileType cwCaptureManager::fileType() const {
    return Filetype;
}

/**
* @brief class::scene
* @return
*/
inline QGraphicsScene* cwCaptureManager::scene() const {
    return Scene;
}

/**
* @brief cwCaptureManager::numberOfCatputers
* @return
*/
inline int cwCaptureManager::numberOfCaptures() const {
    return Captures.size();
}

/**
 * @brief cwCaptureManager::typeNameToFileType
 * @param fileType
 * @return
 */
inline cwCaptureManager::FileType cwCaptureManager::typeNameToFileType(QString fileType) const
{
    return FileTypes.value(fileType, UnknownType);
}

/**
* @brief cwCaptureManager::groupModel
* @return
*/
inline cwCaptureGroupModel* cwCaptureManager::groupModel() const {
    return GroupModel;
}




#endif // CWSCREENCAPTUREMANAGER_H
