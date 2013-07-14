/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSCRAPPOINTVIEW_H
#define CWSCRAPPOINTVIEW_H

//Our includes
#include "cwAbstractPointManager.h"
class cwScrap;
class cwScrapItem;

class cwScrapPointView : public cwAbstractPointManager
{
    Q_OBJECT

    Q_PROPERTY(cwScrap* scrap READ scrap WRITE setScrap NOTIFY scrapChanged)
    Q_PROPERTY(cwScrapItem* scrapItem READ scrapItem WRITE setScrapItem NOTIFY scrapItemChanged)

public:
    explicit cwScrapPointView(QQuickItem *parent = 0);

    cwScrapItem* scrapItem() const;
    void setScrapItem(cwScrapItem* scrapItem);

    cwScrap* scrap() const;
    virtual void setScrap(cwScrap* scrap);

signals:
    void scrapChanged();
    void scrapItemChanged();

public slots:

protected:
    cwScrap* Scrap; //!< The scrap this is class keeps track of

    virtual void updateItemData(QQuickItem* item, int index);

private:

    cwScrapItem* ScrapItem; //!< For selection and holding the scrap
    
};

/**
    Gets scrap that this class renders all the stations of
*/
inline cwScrap* cwScrapPointView::scrap() const {
    return Scrap;
}

/**
  Gets scrapItem
  */
inline cwScrapItem* cwScrapPointView::scrapItem() const {
    return ScrapItem;
}

#endif // CWSCRAPPOINTVIEW_H
