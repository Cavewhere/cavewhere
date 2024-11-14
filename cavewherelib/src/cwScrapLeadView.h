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

    void setPositionRole(cwScrap::LeadDataRole role);
    cwScrap::LeadDataRole positionRole() const;

private:
    virtual QString qmlSource() const;
    virtual void updateItemPosition(QQuickItem* item, int index);

private slots:
    void updateViewWithData(int begin, int end, QList<int> roles);

protected:
//    virtual QSGNode* updatePaintNode(QSGNode * oldNode, UpdatePaintNodeData *);

private:
    cwScrap::LeadDataRole PositionRole; //Should be either LeadPositionOnNote or LeadPosition

};


/**
 * @brief cwScrapLeadView::positionRole
 * @return Returns the position that this view will use to display the lead
 *
 * By default this is cwScrap::LeadPositionOnNote.
 */
inline cwScrap::LeadDataRole cwScrapLeadView::positionRole() const
{
    return PositionRole;
}


#endif // CWSCRAPLEADVIEW_H
