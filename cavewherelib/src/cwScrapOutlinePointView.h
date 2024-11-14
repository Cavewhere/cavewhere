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
    virtual QString qmlSource() const;
    virtual void updateItemPosition(QQuickItem* item, int pointIndex);
    
public slots:

private slots:
    void reset();

    
};

/**
 * @brief cwScrapControlPointView::qmlSource
 * @return Returns the point's qml definition
 */
inline QString cwScrapOutlinePointView::qmlSource() const
{
    return QStringLiteral("qrc:/cavewherelib/cavewherelib/ScrapOutlinePoint.qml");
}


#endif // CWSCRAPCONTROLPOINTVIEW_H
