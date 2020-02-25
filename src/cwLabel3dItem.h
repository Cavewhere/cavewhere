/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWLABEL3DITEM_H
#define CWLABEL3DITEM_H

//Qt includes
#include <QVector3D>
#include <QString>
#include <QFont>

class cwLabel3dItem
{
public:
    cwLabel3dItem();
    cwLabel3dItem(QString text, QVector3D position);

    void setText(QString text);
    QString text() const;

    void setPosition(QVector3D worldCoords);
    QVector3D position() const;

private:
    QFont Font;
    QString Text;
    QVector3D Position;

};

inline void cwLabel3dItem::setText(QString text)
{
    Text = text;
}

inline QString cwLabel3dItem::text() const
{
    return Text;
}

inline void cwLabel3dItem::setPosition(QVector3D worldCoords)
{
    Position = worldCoords;
}

inline QVector3D cwLabel3dItem::position() const
{
    return Position;
}



#endif // CWLABEL3DITEM_H
