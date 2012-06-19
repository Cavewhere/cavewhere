#ifndef CWBASESCRAPINTERACTION_H
#define CWBASESCRAPINTERACTION_H

//Our includes
#include "cwNoteInteraction.h"
class cwScrap;
class cwImageItem;
class cwScrapControlPointView;

class cwBaseScrapInteraction : public cwNoteInteraction
{
    Q_OBJECT

    Q_PROPERTY(cwImageItem* imageItem READ imageItem WRITE setImageItem NOTIFY imageItemChanged)
    Q_PROPERTY(cwScrapControlPointView* controlPointView READ controlPointView WRITE setControlPointView NOTIFY controlPointViewChanged)

public:
    explicit cwBaseScrapInteraction(QQuickItem *parent = 0);

    Q_INVOKABLE void addPoint(QPoint imageCoordinate);

    cwImageItem* imageItem() const;
    void setImageItem(cwImageItem* imageItem);

    cwScrapControlPointView* controlPointView() const;
    void setControlPointView(cwScrapControlPointView* controlPointView);

signals:
    void imageItemChanged();
    void controlPointViewChanged();

public slots:
    void startNewScrap();

private:
    int CurrentScrapIndex;

    cwImageItem* ImageItem; //!< For converting viewport coordinates into note coordinates
    cwScrapControlPointView* ControlPointView; //!< For testing if we clicked on a control point

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
inline cwScrapControlPointView* cwBaseScrapInteraction::controlPointView() const {
    return ControlPointView;
}

#endif // CWBASESCRAPINTERACTION_H
