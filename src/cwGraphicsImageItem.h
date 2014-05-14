/**************************************************************************
**
**    Copyright (C) 2014 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#ifndef CWGRAPHICSIMAGEITEM_H
#define CWGRAPHICSIMAGEITEM_H

//Qt includes
#include <QImage>
#include <QGraphicsItem>

class cwGraphicsImageItem : public QGraphicsItem
{
public:
    explicit cwGraphicsImageItem(QGraphicsItem *parent = 0);

    void setImage(QImage image);
    QImage image() const;

    QRectF boundingRect() const;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget);


signals:

public slots:

private:
    QImage Image;

};

#endif // CWGRAPHICSIMAGEITEM_H
