#ifndef CWSCRAPITEM_H
#define CWSCRAPITEM_H

//Qt includes
#include <QDeclarativeItem>

//Our includes
class cwScrap;

/**
  \brief This draws a scrap
  */
class cwScrapItem : public QDeclarativeItem
{
    Q_OBJECT

    Q_PROPERTY(cwScrap* scrap READ scrap WRITE setScrap NOTIFY scrapChanged)

public:
    explicit cwScrapItem(QDeclarativeItem *parent = 0);

    void setScrap(cwScrap* scrap);
    cwScrap* scrap() const;

protected:
    void paint(QPainter *, const QStyleOptionGraphicsItem *, QWidget *);

signals:
    void scrapChanged();

public slots:

private:
    cwScrap* Scrap;

private slots:
    void updateScrapItem();

};

/**
  Gets the scrap this scrap will scrap item will draw
  */
inline cwScrap* cwScrapItem::scrap() const {
    return Scrap;
}

/**
  Repaints the the scrap
  */
inline void cwScrapItem::updateScrapItem() {
    update();
}

#endif // CWSCRAPITEM_H
