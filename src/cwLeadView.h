/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#ifndef CWLEADVIEW_H
#define CWLEADVIEW_H

//Qt includes
#include <QPointer>

//Our includes
#include "cwAbstractPointManager.h"
class cwCavingRegion;
class cwScrap;

class cwLeadView : public cwAbstractPointManager
{
    Q_OBJECT

    Q_PROPERTY(cwCavingRegion* region READ region WRITE setRegion NOTIFY regionChanged)


public:
    cwLeadView();
    ~cwLeadView();

    cwCavingRegion* region() const;
    void setRegion(cwCavingRegion* region);

protected:
    QUrl qmlSource() const;
    void updateItemData(QQuickItem* item, int pointIndex);
    void updateItemPosition(QQuickItem* item, int pointIndex);

signals:
    void regionChanged();

private:
    QPointer<cwCavingRegion> Region; //!<

    QHash<cwScrap*, int> ScrapIndexOffset; //Each scrap will have a range of valid indexes, we mantain this database

private slots:



};

#endif // CWLEADVIEW_H
