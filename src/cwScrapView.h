#ifndef CWSCRAPVIEW_H
#define CWSCRAPVIEW_H

//Qt includes
#include <QQuickItem>
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
class cwScrapView : public QQuickItem
{
    Q_OBJECT

    Q_PROPERTY(cwNote* note READ note WRITE setNote NOTIFY noteChanged)
    Q_PROPERTY(cwTransformUpdater* transformUpdater READ transformUpdater WRITE setTransformUpdater NOTIFY transformUpdaterChanged)
    Q_PROPERTY(cwScrapItem* selectedScrapItem READ selectedScrapItem  NOTIFY selectScrapIndexChanged)
    Q_PROPERTY(int selectScrapIndex READ selectScrapIndex WRITE setSelectScrapIndex NOTIFY selectScrapIndexChanged)

public:
    explicit cwScrapView(QQuickItem *parent = 0);

    cwNote* note() const;
    void setNote(cwNote* note);

    cwTransformUpdater* transformUpdater() const;
    void setTransformUpdater(cwTransformUpdater* updater);

    cwScrapItem* selectedScrapItem() const;
    void setSelectedScrapItem(cwScrapItem* selectedScrapItem);

    Q_INVOKABLE void clearSelection();
    int selectScrapIndex() const;
    void setSelectScrapIndex(int selectScrapIndex);
    Q_INVOKABLE void selectScrapAt(QPointF notePoint);
    void setSelectScrap(cwScrap* scrap);

    QList<cwScrapItem*> scrapItemsAt(QPointF notePoint);
    cwScrapItem* scrapItemAt(int index);

signals:
    void noteChanged();
    void transformUpdaterChanged();
    void selectScrapIndexChanged();

public slots:

private slots:
    void addScrapItem();
    void updateAllScraps();
    void updateSelection();

private:
    cwNote* Note;

    QList<cwScrapItem*> ScrapItems;
    int SelectScrapIndex; //!< The current select scrap, -1 if no scrapitem is selected

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
  Clears the current selection
  */
inline void cwScrapView::clearSelection() {
    setSelectScrapIndex(-1);
}

/**
Gets selectScrapIndex
*/
inline int cwScrapView::selectScrapIndex() const {
    return SelectScrapIndex;
}
#endif // CWSCRAPVIEW_H
