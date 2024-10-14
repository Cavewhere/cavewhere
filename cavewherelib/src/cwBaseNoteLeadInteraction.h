/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#ifndef CWBASENOTELEADINTERACTION_H
#define CWBASENOTELEADINTERACTION_H

//Our includes
#include "cwBaseNotePointInteraction.h"

//Qt includes
#include <QQmlEngine>

class cwBaseNoteLeadInteraction : public cwBaseNotePointInteraction
{
    Q_OBJECT
    QML_NAMED_ELEMENT(BaseNoteLeadInteraction)

public:
    cwBaseNoteLeadInteraction(QQuickItem* parent = 0);
    ~cwBaseNoteLeadInteraction();

signals:

public slots:

protected:
    void addPoint(QPointF notePosition, cwScrapItem *scrapItem);
};

#endif // CWBASENOTELEADINTERACTION_H
