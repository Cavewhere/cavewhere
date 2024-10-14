/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWPANZOOMINTERACTION_H
#define CWPANZOOMINTERACTION_H

//Our includes
#include "cwInteraction.h"
#include "cwCamera.h"

//Qt includes
#include <QQmlEngine>

class cwBasePanZoomInteraction : public cwInteraction
{
    Q_OBJECT
    QML_NAMED_ELEMENT(BasePanZoomInteraction)

    Q_PROPERTY(cwCamera* camera READ camera WRITE setCamera NOTIFY cameraChanged)

public:
    explicit cwBasePanZoomInteraction(QQuickItem *parent = 0);

    void setCamera(cwCamera* camera);
    cwCamera* camera() const;

        //For panning notes
    Q_INVOKABLE void panFirstPoint(QPointF firstMousePoint);
    Q_INVOKABLE void panMove(QPointF mousePosition);
    Q_INVOKABLE void zoom(int delta, QPointF position);

signals:
    void cameraChanged();

public slots:

protected:
//    void wheelEvent(QGraphicsSceneWheelEvent *event);

private:
    cwCamera* Camera;

    //For interaction
     QPoint LastPanPoint;

};

/**

  */
inline cwCamera* cwBasePanZoomInteraction::camera() const {
    return Camera;
}
#endif // CWPANZOOMINTERACTION_H
