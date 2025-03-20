/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#ifndef CWSCRAPLEADVIEW_H
#define CWSCRAPLEADVIEW_H

//Our includes
#include "cwScrapPointView.h"
#include "cwScrap.h"
#include "cwGlobalDirectory.h"

//Qt includes
#include <QQuickItem>
#include <QQmlEngine>

class cwScrapLeadView : public cwScrapPointView
{
    Q_OBJECT
    QML_NAMED_ELEMENT(ScrapLeadView)

public:
    cwScrapLeadView(QQuickItem* parent = 0);
    ~cwScrapLeadView();

    void setScrap(cwScrap* scrap);

private:
    virtual QString qmlSource() const;
    virtual void updateItemPosition(QQuickItem* item, int index);

private slots:
    void updateViewWithData(int begin, int end, QList<int> roles);


private:
    cwScrap::LeadDataRole PositionRole; //Should be either LeadPositionOnNote or LeadPosition

};


#endif // CWSCRAPLEADVIEW_H
