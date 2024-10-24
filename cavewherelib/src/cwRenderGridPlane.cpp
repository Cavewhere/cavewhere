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
    cwRenderObject(parent),
    m_plane(QPlane3D(QVector3D(0.0, 0.0, -75.0), QVector3D(0.0, 0.0, 1.0))),
    m_extent(3000.0) //3km in the negitive and positive direction from origin
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

    updateModelMatrix();
}

cwRHIObject* cwRenderGridPlane::createRHIObject()
{
    return new cwRHIGridPlane();
}

void cwRenderGridPlane::setPlane(QPlane3D plane) {
    if(plane != m_plane) {
        plane = m_plane;
        updateModelMatrix();
    }
}


/**
    Sets extent, the max rendering geometry of the plane
*/
void cwRenderGridPlane::setExtent(double extent) {
    if(m_extent != extent) {
        m_extent = extent;
        updateModelMatrix();
        emit extentChanged();
    }
}



/**
 * @brief cwRenderGridPlane::updateModelMatrix
 *
 * Updates the model index for the grid plane
 */
void cwRenderGridPlane::updateModelMatrix() {

    QMatrix4x4 modelMatrix;

    //Position
    modelMatrix.translate(m_plane.origin());

    //Scale
    modelMatrix.scale(m_extent);

//    //Rotation
//    QVector3D upVector(0.0, 0.0, 1.0);
//    QVector3D planeNormal = Plane.normal().normalized();

//    //Find the rotation from up vector
//    double rotation = acos(QVector3D::dotProduct(upVector, planeNormal)) * cwGlobals::radiansToDegrees();

//    //Find the cross product between the vectors
//    QVector3D cross = QVector3D::crossProduct(upVector, planeNormal);

//    modelMatrix.rotate(rotation, cross);

    m_modelMatrix.setValue(modelMatrix);
}
