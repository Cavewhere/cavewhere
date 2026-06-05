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
    Q_PROPERTY(QVector3D eyeOffset MEMBER eyeOffset)

public:
    QVector3D center;
    double azimuth = 0.0;
    double pitch = 90.0;
    double distance = 50.0;
    double zoomScale = 1.0;

    //! Eye-space translation applied LAST in the canonical view recipe.
    //! Captures any user pan that has shifted m_center off the screen
    //! centre while preserving orbit semantics — rotation still pivots
    //! around m_center, the pan just slides the view sideways/up/down.
    //! (0,0,0) means "m_center sits at view-space (0,0,-distance)", i.e.
    //! the screen-centred default. Z is reserved (always zero); the
    //! along-eye depth lives in the distance channel.
    QVector3D eyeOffset;

    bool operator==(const cwTurnTableViewState&) const = default;
};

Q_DECLARE_METATYPE(cwTurnTableViewState)
Q_DECLARE_TYPEINFO(cwTurnTableViewState, Q_RELOCATABLE_TYPE);

#endif // CWTURNTABLEVIEWSTATE_H
