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

        //Position
        modelMatrix.translate(m_plane.value().origin());

        //Scale the plane
        modelMatrix.scale(m_extent);

        return modelMatrix;
    });

    //For updating the backend, the backend get data on a seperate thread
    m_modelMatrix.setValue(m_modelMatrixProperty.value());
    m_modelMatrixProperty.onValueChanged([this]() {
        m_modelMatrix.setValue(m_modelMatrixProperty.value());
    });
}

cwRHIObject* cwRenderGridPlane::createRHIObject()
{
    return new cwRHIGridPlane();
}
