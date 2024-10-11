/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#ifndef CWBASENOTEPOINTINTERACTION_H
#define CWBASENOTEPOINTINTERACTION_H

//Qt includes
#include <QObject>
#include <QPointer>

//Our includes
#include "cwInteraction.h"
#include "cwScrapView.h"
class cwScrapItem;
class cwScrap;

class cwBaseNotePointInteraction : public cwInteraction
{
    Q_OBJECT

    Q_PROPERTY(cwScrapView* scrapView READ scrapView WRITE setScrapView NOTIFY scrapViewChanged)

public:
    cwBaseNotePointInteraction(QQuickItem* parent = nullptr);
    ~cwBaseNotePointInteraction();

    cwScrapView* scrapView() const;
    void setScrapView(cwScrapView* scrapView);

    Q_INVOKABLE void addPoint(QPointF notePosition);

protected:
    virtual void addPoint(QPointF notePosition, cwScrapItem* scrapItem) = 0;

signals:
    void scrapViewChanged();

private:
    QPointer<cwScrapView> ScrapView;

    cwScrapItem* selectScrapForAdding(QList<cwScrapItem*> scrapItems);

};

#endif // CWBASENOTEPOINTINTERACTION_H
