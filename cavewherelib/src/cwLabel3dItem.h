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
    cwLabel3dItem(const cwLabel3dItem& other) = default;

    void setText(QString text);
    QString text() const;

    void setPosition(QVector3D worldCoords);
    QVector3D position() const;

    void setWasVisible(int wasVisible);
    int wasVisible() const;

    void setVisibleStreak(int visibleStreak);
    int visibleStreak() const;

private:
    QFont Font;
    QString Text;
    QVector3D Position;
    int WasVisible = 0;
    int VisibleStreak = std::numeric_limits<int>::max();

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

inline void cwLabel3dItem::setWasVisible(int wasVisible)
{
    WasVisible = wasVisible;
}

inline int cwLabel3dItem::wasVisible() const
{
    return WasVisible;
}

inline void cwLabel3dItem::setVisibleStreak(int visibleStreak)
{
    VisibleStreak = visibleStreak;
}

inline int cwLabel3dItem::visibleStreak() const
{
    return VisibleStreak;
}



#endif // CWLABEL3DITEM_H
