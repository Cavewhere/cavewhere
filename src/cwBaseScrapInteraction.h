#ifndef CWBASESCRAPINTERACTION_H
#define CWBASESCRAPINTERACTION_H

//Our includes
#include "cwNoteInteraction.h"
class cwScrap;
class cwImageItem;
class cwScrapOutlinePointView;

class cwBaseScrapInteraction : public cwNoteInteraction
{
    Q_OBJECT

    Q_PROPERTY(cwImageItem* imageItem READ imageItem WRITE setImageItem NOTIFY imageItemChanged)
    Q_PROPERTY(cwScrapOutlinePointView* outlinePointView READ outlinePointView WRITE setOutlinePointView NOTIFY outlinePointViewChanged)
    Q_PROPERTY(cwScrap* scrap READ scrap WRITE setScrap NOTIFY scrapChanged)

public:
    explicit cwBaseScrapInteraction(QQuickItem *parent = 0);

    Q_INVOKABLE void addPoint(QPoint imageCoordinate);

    cwImageItem* imageItem() const;
    void setImageItem(cwImageItem* imageItem);

    cwScrapOutlinePointView* outlinePointView() const;
    void setOutlinePointView(cwScrapOutlinePointView* outlinePointView);

    Q_INVOKABLE void addScrap();
    void setScrap(cwScrap* scrap);
    cwScrap* scrap() const;

    Q_INVOKABLE QVariantMap snapToScrapLine(QPoint qtViewportPosition) const;

signals:
    void imageItemChanged();
    void outlinePointViewChanged();
    void scrapChanged();

public slots:
    void startNewScrap();

private:
    class cwClosestPoint {
    public:
        cwClosestPoint() : IsValid(false), InsertIndex(-1) { }
        cwClosestPoint(QPointF closestPoint, int insertIndex) :
            IsValid(true),
            ClosestPoint(closestPoint),
            InsertIndex(insertIndex) { }

        bool IsValid;
        QPointF ClosestPoint;
        int InsertIndex;
    };

    cwScrap* Scrap;
    cwImageItem* ImageItem; //!< For converting viewport coordinates into note coordinates
    cwScrapOutlinePointView* OutlinePointView; //!< For testing if we clicked on a control point

    void closeCurrentScrap();

    cwClosestPoint calcClosestPoint(QPoint qtViewportPosition) const;

private slots:
    void deactivating();
    void scrapDeleted();

};
/**
Gets imageItem
*/
inline cwImageItem* cwBaseScrapInteraction::imageItem() const {
    return ImageItem;
}

/**
Gets controlPointView
*/
inline cwScrapOutlinePointView* cwBaseScrapInteraction::outlinePointView() const {
    return OutlinePointView;
}

/**
 * @brief cwBaseScrapInteraction::scrap
 * @return The current scrap of this interaction
 */
inline cwScrap *cwBaseScrapInteraction::scrap() const
{
    return Scrap;
}

/**
 * @brief cwBaseScrapInteraction::scrapDeleted
 *
 * Called when the current scrap is destroyed
 */
inline void cwBaseScrapInteraction::scrapDeleted() {
    Scrap = NULL;
}


#endif // CWBASESCRAPINTERACTION_H
