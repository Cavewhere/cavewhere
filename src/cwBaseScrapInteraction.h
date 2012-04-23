#ifndef CWBASESCRAPINTERACTION_H
#define CWBASESCRAPINTERACTION_H

//Our includes
#include "cwNoteInteraction.h"
class cwScrap;

class cwBaseScrapInteraction : public cwNoteInteraction
{
    Q_OBJECT



public:
    explicit cwBaseScrapInteraction(QQuickItem *parent = 0);

    Q_INVOKABLE void addPoint(QPointF noteCoordinate);

signals:

public slots:
    void startNewScrap();

private:
    int CurrentScrapIndex;

    void addScrap();

};

#endif // CWBASESCRAPINTERACTION_H
