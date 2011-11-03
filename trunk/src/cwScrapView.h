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

public:
    explicit cwScrapView(QDeclarativeItem *parent = 0);

    cwNote* note() const;
    void setNote(cwNote* note);

    cwTransformUpdater* transformUpdater() const;
    void setTransformUpdater(cwTransformUpdater* updater);

    cwScrapItem* selectedScrap() const;
    void setSelectedScrap(cwScrapItem* selectedScrap);

    Q_INVOKABLE void clearSelection();
    Q_INVOKABLE void selectScrapAt(QPointF notePoint);
    void selectScrapItem(cwScrapItem* scrapItem);

    QList<cwScrapItem*> scrapItemsAt(QPointF notePoint);

signals:
    void noteChanged();
    void transformUpdaterChanged();
    void selectedScrapChanged();

public slots:

private slots:
    void addScrapItem();
    void updateAllScraps();

private:
    cwNote* Note;

    QList<cwScrapItem*> ScrapItems;
    cwScrapItem* SelectedScrap; //!< The current select scrap, NULL if no scrapitem is selected

    cwTransformUpdater* TransformUpdater;

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

/**
  Clears the current selection
  */
inline void cwScrapView::clearSelection() {
    setSelectedScrap(NULL);
}

#endif // CWSCRAPVIEW_H
