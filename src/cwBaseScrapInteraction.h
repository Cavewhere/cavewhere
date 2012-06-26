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
    Q_PROPERTY(cwScrapOutlinePointView* controlPointView READ controlPointView WRITE setControlPointView NOTIFY controlPointViewChanged)


public:
    explicit cwBaseScrapInteraction(QQuickItem *parent = 0);

    Q_INVOKABLE void addPoint(QPoint imageCoordinate);

    cwImageItem* imageItem() const;
    void setImageItem(cwImageItem* imageItem);

    cwScrapOutlinePointView* controlPointView() const;
    void setControlPointView(cwScrapOutlinePointView* controlPointView);

    void setScrap(cwScrap* scrap);

signals:
    void imageItemChanged();
    void controlPointViewChanged();

public slots:
    void startNewScrap();

private:
    cwScrap* Scrap;
    cwImageItem* ImageItem; //!< For converting viewport coordinates into note coordinates
    cwScrapOutlinePointView* ControlPointView; //!< For testing if we clicked on a control point

    void addScrap();
    void closeCurrentScrap();

private slots:
    void deactivating();

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
inline cwScrapOutlinePointView* cwBaseScrapInteraction::controlPointView() const {
    return ControlPointView;
}

#endif // CWBASESCRAPINTERACTION_H
