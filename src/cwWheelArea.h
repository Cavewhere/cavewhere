/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWWHEELAREA_H
#define CWWHEELAREA_H

#include <QDeclarativeItem>

class cwWheelArea : public QDeclarativeItem
{
    Q_OBJECT

    Q_PROPERTY(QPointF position READ position)

public:
    explicit cwWheelArea(QDeclarativeItem *parent = 0);

    QPointF position() const;

signals:
    void verticalScroll(int delta);

protected:
    void wheelEvent(QGraphicsSceneWheelEvent *event);

private:
    QPointF LastPosition;

public slots:

};

inline QPointF cwWheelArea::position() const {
    return LastPosition;
}

#endif // CWWHEELAREA_H
