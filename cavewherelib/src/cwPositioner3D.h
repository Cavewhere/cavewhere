/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWPOSITIONER3D_H
#define CWPOSITIONER3D_H

//Qt includes
#include <QQuickItem>
#include <QVector3D>
#include <QQmlEngine>

//Our includes
#include "cwGlobals.h"

class CAVEWHERE_LIB_EXPORT cwPositioner3D : public QQuickItem
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Positioner3D)

    Q_PROPERTY(QVector3D position3D READ position3D WRITE setPosition3D NOTIFY position3DChanged)

public:
    explicit cwPositioner3D(QQuickItem *parent = 0);

    QVector3D position3D() const;
    void setPosition3D(QVector3D position3D);

signals:
    void position3DChanged();

public slots:

private:
    QVector3D Position3D; //!<

};

/**
Gets position3D
*/
inline QVector3D cwPositioner3D::position3D() const {
    return Position3D;
}


#endif // CWPOSITIONER3D_H
