/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwRenderGridPlane.h"
#include "cwCamera.h"
#include "cwRHIGridPlane.h"

//Qt includes
#include <QQuaternion>
#include <QVector3D>

//Std includes
#include <math.h>

cwRenderGridPlane::cwRenderGridPlane(QObject* parent) :
    cwRenderObject(parent)
{
    connect(this, &cwRenderObject::sceneChange, this, [this]() {
        //Disconnect the scene and camera
        disconnect(m_sceneConnection);
        disconnect(m_viewMatrixConnection);
        disconnect(m_projectionConnection);

        if(scene()) {

            auto connectToNewCamera = [this]() {
                //Disconnect the old camera
                disconnect(m_viewMatrixConnection);
                disconnect(m_projectionConnection);

                if(scene()->camera()) {
                    m_viewMatrixConnection = connect(scene()->camera(), &cwCamera::viewMatrixChanged, this, [this]() {
                        update();
                    });
                    m_projectionConnection = connect(scene()->camera(), &cwCamera::projectionChanged, this, [this]() {
                        update();
                    });

                }

            };

            m_sceneConnection = connect(scene(), &cwScene::cameraChanged, connectToNewCamera);
            connectToNewCamera();
        }
    });

    m_modelMatrixProperty.setBinding([this](){
        QMatrix4x4 modelMatrix;
        const QPlane3D plane = m_plane.value();

        //Position
        modelMatrix.translate(plane.origin());

        //Rotate so that the plane's local +Z aligns with the plane normal
        const QVector3D normal = plane.normal();
        if (!normal.isNull()) {
            const QVector3D defaultNormal(0.0f, 0.0f, 1.0f);
            const QQuaternion rotation = QQuaternion::rotationTo(defaultNormal, normal.normalized());
            modelMatrix.rotate(rotation);
        }

        //Scale the plane
        modelMatrix.scale(m_extent);

        return modelMatrix;
    });

    m_scaleMatrixProperty.setBinding([this]() {
        QMatrix4x4 scaleMatrix;
        scaleMatrix.scale(m_extent.value());
        return scaleMatrix;
    });

    //For updating the backend, the backend get data on a seperate thread
    m_modelMatrix.setValue(m_modelMatrixProperty.value());
    m_modelMatrixNotifier = m_modelMatrixProperty.addNotifier([this]() {
        m_modelMatrix.setValue(m_modelMatrixProperty.value());
    });

    m_scaleMatrix.setValue(m_scaleMatrixProperty.value());
    m_scaleMatrixNotifier = m_scaleMatrixProperty.addNotifier([this]() {
        m_scaleMatrix.setValue(m_scaleMatrixProperty.value());
    });
}

cwRHIObject* cwRenderGridPlane::createRHIObject()
{
    return new cwRHIGridPlane();
}
