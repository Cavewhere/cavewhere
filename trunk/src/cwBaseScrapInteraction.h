#ifndef CWBASESCRAPINTERACTION_H
#define CWBASESCRAPINTERACTION_H

//Qt includes
#include <QDeclarativeItem>

//Our includes
#include <cwBasePanZoomInteraction.h>

class cwBaseScrapInteraction : public cwBasePanZoomInteraction
{
    Q_OBJECT
public:
    explicit cwBaseScrapInteraction(QDeclarativeItem *parent = 0);

    Q_INVOKABLE void addScrap();
    Q_INVOKABLE void addPoint(int scrapIndex, QPointF qtViewportCoordinate);

signals:

public slots:

};

#endif // CWBASESCRAPINTERACTION_H
