/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSCRAPVIEW_H
#define CWSCRAPVIEW_H

//Qt includes
#include <QQuickItem>
#include <QGraphicsPolygonItem>
#include <QQmlEngine>

//Our includes
class cwNote;
class cwCamera;
class cwScrap;
class cwScrapItem;
class cwSelectionManager;

/**
  This class visualizes scraps in NoteItem
  */
class cwScrapView : public QQuickItem
{
    Q_OBJECT
    QML_NAMED_ELEMENT(ScrapView)

    Q_PROPERTY(cwNote* note READ note WRITE setNote NOTIFY noteChanged)
    // Q_PROPERTY(cwTransformItemUpdater* transformUpdater READ transformUpdater WRITE setTransformUpdater NOTIFY transformUpdaterChanged)
    Q_PROPERTY(cwScrapItem* selectedScrapItem READ selectedScrapItem  NOTIFY selectScrapIndexChanged)
    Q_PROPERTY(int selectScrapIndex READ selectScrapIndex WRITE setSelectScrapIndex NOTIFY selectScrapIndexChanged)
    Q_PROPERTY(double zoom READ zoom WRITE setZoom NOTIFY zoomChanged);


public:
    explicit cwScrapView(QQuickItem *parent = 0);

    cwNote* note() const;
    void setNote(cwNote* note);

    // cwTransformItemUpdater* transformUpdater() const;
    // void setTransformUpdater(cwTransformItemUpdater* updater);

    cwScrapItem* selectedScrapItem() const;
    void setSelectedScrapItem(cwScrapItem* selectedScrapItem);

    Q_INVOKABLE void clearSelection();
    int selectScrapIndex() const;
    void setSelectScrapIndex(int selectScrapIndex);
    Q_INVOKABLE void selectScrapAt(QPointF imagePoint);
    void setSelectScrap(cwScrap* scrap);

    QList<cwScrapItem*> scrapItemsAt(QPointF notePoint);
    cwScrapItem* scrapItemAt(int index);
    int indexOf(cwScrapItem* item) const;

    double zoom() const { return m_zoom; }
    void setZoom(double zoom);

    static QTransform toImage(cwNote* note);
    static QTransform toNormalized(cwNote* note);

signals:
    void noteChanged();
    void transformUpdaterChanged();
    void selectScrapIndexChanged();
    void zoomChanged();

public slots:

private slots:
    void insertScrapItem(int begin, int end);
    void updateAllScraps();
    void updateSelection();
    void updateRemovedScraps(int begin, int end);

private:
    cwNote* m_note;

    QList<cwScrapItem*> m_scrapItems;
    int m_selectScrapIndex = -1; //!< The current select scrap, -1 if no scrapitem is selected

    cwSelectionManager* m_selectionManager;

    double m_zoom;

};

/**
  Gets the note for the cwScrapView
  */
inline cwNote* cwScrapView::note() const {
    return m_note;
}

// /**
//   Gets the transform updater
//   */
// inline cwTransformItemUpdater* cwScrapView::transformUpdater() const {
//     return m_transformUpdater;
// }

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
    return m_selectScrapIndex;
}

/**
 * @brief cwScrapView::indexOf
 * @param item - The item
 * @return Get's the index of the item in the scrap view
 */
inline int cwScrapView::indexOf(cwScrapItem *item) const
{
    return m_scrapItems.indexOf(item);
}

#endif // CWSCRAPVIEW_H
