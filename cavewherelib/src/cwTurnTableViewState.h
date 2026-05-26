/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWTURNTABLEVIEWSTATE_H
#define CWTURNTABLEVIEWSTATE_H

//Qt includes
#include <QMetaType>
#include <QQmlEngine>
#include <QVector3D>

class cwTurnTableViewState
{
    Q_GADGET
    QML_VALUE_TYPE(turnTableViewState)

    Q_PROPERTY(QVector3D center MEMBER center)
    Q_PROPERTY(double azimuth MEMBER azimuth)
    Q_PROPERTY(double pitch MEMBER pitch)
    Q_PROPERTY(double distance MEMBER distance)
    Q_PROPERTY(double zoomScale MEMBER zoomScale)

public:
    QVector3D center;
    double azimuth = 0.0;
    double pitch = 90.0;
    double distance = 50.0;
    double zoomScale = 1.0;

    bool operator==(const cwTurnTableViewState&) const = default;
};

Q_DECLARE_METATYPE(cwTurnTableViewState)
Q_DECLARE_TYPEINFO(cwTurnTableViewState, Q_RELOCATABLE_TYPE);

#endif // CWTURNTABLEVIEWSTATE_H
