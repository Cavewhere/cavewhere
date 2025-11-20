/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSCRAPCONTROLPOINTVIEW_H
#define CWSCRAPCONTROLPOINTVIEW_H

//Our includes
#include "cwScrapPointView.h"
#include "cwGlobalDirectory.h"
class cwScrap;

//Qt includes
#include <QQmlEngine>

class cwScrapOutlinePointView : public cwScrapPointView
{
    Q_OBJECT
    QML_NAMED_ELEMENT(ScrapOutlinePointView)

public:
    explicit cwScrapOutlinePointView(QQuickItem *parent = 0);
    
    void setScrap(cwScrap* scrap);

protected:
    virtual void updateItemPosition(QQuickItem* item, int pointIndex);
    
public slots:

private slots:
    void reset();

    
};


#endif // CWSCRAPCONTROLPOINTVIEW_H
