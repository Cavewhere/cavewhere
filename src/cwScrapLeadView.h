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
#include "cwGlobalDirectory.h"

//Qt includes
#include <QQuickItem>

class cwScrapLeadView : public cwScrapPointView
{
    Q_OBJECT

public:
    cwScrapLeadView(QQuickItem* parent = 0);
    ~cwScrapLeadView();

    void setScrap(cwScrap* scrap);

private:
    virtual QUrl qmlSource() const;
    virtual void updateItemPosition(QQuickItem* item, int index);

private slots:
    void updateViewWithData(int begin, int end, QList<int> roles);

protected:
//    virtual QSGNode* updatePaintNode(QSGNode * oldNode, UpdatePaintNodeData *);
};

/**
 * @brief cwScrapStationView::qmlSource
 * @return The qml source of the point that'll be rendered
 */
inline QUrl cwScrapLeadView::qmlSource() const
{
    return QUrl::fromLocalFile(cwGlobalDirectory::baseDirectory() + "qml/NoteLead.qml");
}

#endif // CWSCRAPLEADVIEW_H
