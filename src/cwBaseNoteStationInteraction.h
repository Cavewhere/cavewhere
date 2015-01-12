/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWBASENOTESTATIONINTERACTION_H
#define CWBASENOTESTATIONINTERACTION_H

//Qt includes
#include <QQuickItem>

//Our includes
#include "cwBaseNotePointInteraction.h"

class cwBaseNoteStationInteraction : public cwBaseNotePointInteraction
{
    Q_OBJECT

public:
    explicit cwBaseNoteStationInteraction(QQuickItem *parent = 0);

public slots:

protected:
    void addPoint(QPointF notePosition, cwScrapItem* scrapItem);

private:

};



#endif // CWBASENOTESTATIONINTERACTION_H
