#ifndef CWSCRAPVIEW_H
#define CWSCRAPVIEW_H

//Qt includes
#include <QDeclarativeItem>
#include <QGraphicsPolygonItem>

//Our includes
class cwNote;
class cwCamera;
class cwScrap;
class cwTransformUpdater;
class cwScrapItem;
#include "cwTransformUpdater.h"

/**
  This class visualizes scraps in NoteItem
  */
class cwScrapView : public QDeclarativeItem
{
    Q_OBJECT

    Q_PROPERTY(cwNote* note READ note WRITE setNote NOTIFY noteChanged)
    Q_PROPERTY(cwTransformUpdater* transformUpdater READ transformUpdater WRITE setTransformUpdater NOTIFY transformUpdaterChanged)
    Q_PROPERTY(cwScrapItem* selectedScrap READ selectedScrap WRITE setSelectedScrap NOTIFY selectedScrapChanged)

//    Q_PROPERTY(QMatrix4x4 noteMatrix READ noteMatrix WRITE setNoteMatrix NOTIFY noteMatrixChanged)
//    Q_PROPERTY(cwCamera* camera READ camera WRITE setCamera NOTIFY cameraChanged)

public:
    explicit cwScrapView(QDeclarativeItem *parent = 0);

    cwNote* note() const;
    void setNote(cwNote* note);

    cwTransformUpdater* transformUpdater() const;
    void setTransformUpdater(cwTransformUpdater* updater);

    cwScrapItem* selectedScrap() const;
    void setSelectedScrap(cwScrapItem* selectedScrap);

    void selectScrapItem(cwScrapItem* scrapItem);

    QList<cwScrapItem*> scrapItemsAt(QPointF notePoint);

//    QMatrix4x4 noteMatrix() const;
//    void setNoteMatrix(QMatrix4x4 matrix);

//    cwCamera* camera() const;
//    void setCamera(cwCamera* camera);

signals:
    void noteChanged();
    void transformUpdaterChanged();
    void selectedScrapChanged();
//    void noteMatrixChanged();
//    void cameraChanged();

public slots:

private slots:
    void addScrapItem();
//    void updateScrapGeometry();
//    void updateScrapGeometry(cwScrap* scrap);
    void updateAllScraps();

private:
    cwNote* Note;

    QList<cwScrapItem*> ScrapItems;
    cwScrapItem* SelectedScrap; //!< The current select scrap, NULL if no scrapitem is selected

    cwTransformUpdater* TransformUpdater;

    QGraphicsPolygonItem* createPolygon();
};

/**
  Gets the note for the cwScrapView
  */
inline cwNote* cwScrapView::note() const {
    return Note;
}

/**
  Gets the transform updater
  */
inline cwTransformUpdater* cwScrapView::transformUpdater() const {
    return TransformUpdater;
}

/**
Gets selectedScrap
*/
inline cwScrapItem* cwScrapView::selectedScrap() const {
    return SelectedScrap;
}

//inline QMatrix4x4 cwScrapView::noteMatrix() const {
//    return TransformUpdater->modelMatrix();
//}

//inline cwCamera* cwScrapView::camera() const {
//    return TransformUpdater->camera();
//}

//inline void cwScrapView::setNoteMatrix(QMatrix4x4 matrix) {
//    TransformUpdater->setModelMatrix(matrix);
//}

//inline void cwScrapView::setCamera(cwCamera* camera) {
//    TransformUpdater->setCamera(camera);
//}

#endif // CWSCRAPVIEW_H
