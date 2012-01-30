#ifndef CWGLMAPPERITEM_H
#define CWGLMAPPERITEM_H

//Qt includes
#include <QDeclarativeItem>
#include <QMatrix4x4>
#include <QVector3D>

//Our includes
#include "cwCamera.h"

/**

  */
class cwGLMapperItem : public QDeclarativeItem
{
    Q_OBJECT

    Q_PROPERTY(QVector3D position READ position WRITE setPosition NOTIFY positionChanged)

public:
    explicit cwGLMapperItem(QDeclarativeItem *parent = 0);

    QVector3D position() const;
    void setPosition(QVector3D position);

signals:
    void positionChanged();

public slots:

private:
    QVector3D Position;

};

inline QVector3D cwGLMapperItem::position() const {
    return Position;
}

#endif // CWGLMAPPERITEM_H
