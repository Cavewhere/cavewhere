#ifndef CWSCRAPITEM_H
#define CWSCRAPITEM_H

//Qt includes
#include <QDeclarativeItem>

//Our includes
class cwScrap;
class cwTransformUpdater;

/**
  \brief This draws a scrap
  */
class cwScrapItem : public QDeclarativeItem
{
    Q_OBJECT

    Q_PROPERTY(cwScrap* scrap READ scrap WRITE setScrap NOTIFY scrapChanged)
    Q_PROPERTY(bool selected READ isSelected WRITE setSelected NOTIFY selectedChanged)

public:
    explicit cwScrapItem(QDeclarativeItem *parent = 0);

    void setScrap(cwScrap* scrap);
    cwScrap* scrap() const;

    bool isSelected() const;
    void setSelected(bool selected);
signals:
    void scrapChanged();
    void selectedChanged();

public slots:

private:
    //Data class
    cwScrap* Scrap;

    //Visual elements
    QGraphicsPolygonItem* BorderItem;

    bool Selected; //!< True if the scrap is select and false if it isn't

private slots:
    void updateScrapGeometry();

};

/**
  Gets the scrap this scrap will scrap item will draw
  */
inline cwScrap* cwScrapItem::scrap() const {
    return Scrap;
}


/**
  Returns true if the scrap item is select and false if it isn't
  */
inline bool cwScrapItem::isSelected() const {
    return Selected;
}


#endif // CWSCRAPITEM_H
