/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSCENEPICKER_H
#define CWSCENEPICKER_H

//Our includes
#include "cwInteraction.h"
#include "cwScenePick.h"
class cwCamera;
class cwScene;

//Qt includes
#include <QPointer>
#include <QPointF>

/**
 * Base for interactions that turn a screen-space click into a snapped world
 * point. Owns the camera and scene and funnels every pick through a single
 * snapPick(), so the ray-cast, the station snap, and the physical pick
 * tolerance can't drift between the coordinate picker and the measurement tool.
 *
 * Not instantiable on its own; concrete pickers derive from it and add their
 * own readout state.
 */
class cwScenePicker : public cwInteraction
{
    Q_OBJECT
    // Not creatable from QML: it's the shared base for CoordinatePicker /
    // MeasurementInteraction, which inherit its camera and scene properties.
    // QML_ANONYMOUS registers it so those inherited properties participate
    // explicitly in the QML type system.
    QML_ANONYMOUS

    Q_PROPERTY(cwCamera* camera READ camera WRITE setCamera NOTIFY cameraChanged)
    Q_PROPERTY(cwScene* scene READ scene WRITE setScene NOTIFY sceneChanged)

public:
    explicit cwScenePicker(QQuickItem* parent = nullptr);

    // Out-of-line: the QPointer-to-pointer conversion needs the complete type,
    // which only cwScenePicker.cpp includes — derived .cpp files forward-declare.
    cwCamera* camera() const;
    void setCamera(cwCamera* camera);

    cwScene* scene() const;
    void setScene(cwScene* scene);

signals:
    void cameraChanged();
    void sceneChanged();

protected:
    //! Ray-casts screenPoint (Qt viewport pixels) against the scene and snaps to
    //! the nearest survey station within the shared pick tolerance. Returns a
    //! default Result (hit == false) when the camera, scene, or its intersecter
    //! is unavailable.
    cwScenePick::Result snapPick(QPointF screenPoint) const;

private:
    QPointer<cwCamera> m_camera;
    QPointer<cwScene> m_scene;
};

#endif // CWSCENEPICKER_H
