//Our includes
#include "cwBasePanZoomInteraction.h"

//Qt includes
#include <QGraphicsSceneWheelEvent>

cwBasePanZoomInteraction::cwBasePanZoomInteraction(QDeclarativeItem *parent) :
    cwInteraction(parent),
    Camera(new cwCamera(this))
{

}

/**
  \brief Sets the camera for the pan and zoom interaction
  */
void cwBasePanZoomInteraction::setCamera(cwCamera* camera) {
    if(Camera != camera) {

        if(Camera->parent() == this) {
            //Delete camera's that are owned my this object
            Camera->deleteLater();
        }

        Camera = camera;
        emit cameraChanged();
    }
}


/**
  \brief Initializes the pan with the first point in the pan
  */
 void cwBasePanZoomInteraction::panFirstPoint(QPointF firstMousePoint) {
     LastPanPoint = Camera->mapToGLViewport(firstMousePoint.toPoint());
     Camera->unProject(LastPanPoint, 0.1f);
 }

 /**
   \brief Called when the user moves the mouse in the pan area

   This will pan the note
   */
 void cwBasePanZoomInteraction::panMove(QPointF mousePosition) {
     QPoint currentPanPoint = Camera->mapToGLViewport(mousePosition.toPoint());

     QVector3D unProjectedCurrent = Camera->unProject(currentPanPoint, 0.1f);
     QVector3D unProjectedLast = Camera->unProject(LastPanPoint, 0.1f);

     QVector3D delta = unProjectedCurrent - unProjectedLast;

     QMatrix4x4 newMatrix = Camera->viewMatrix();
     newMatrix.translate(delta);
     Camera->setViewMatrix(newMatrix);

     LastPanPoint = currentPanPoint;
 }

 /**
   \brief zooms in or out depending on what the user does.
   */
 void cwBasePanZoomInteraction::zoom(int delta, QPointF position) {
     //Calc the scaleFactor
     float scaleFactor = 1.1f;
     if(delta < 0) {
         //Zoom out
         scaleFactor = 1.0f / scaleFactor;
     }

     QPoint screenCoords = Camera->mapToGLViewport(position.toPoint());
     QVector3D beforeZoom = Camera->unProject(screenCoords, 0.1f);

     //Get the new zoomed matrix
     QMatrix4x4 zoom;
     zoom.scale(scaleFactor, scaleFactor);
     QMatrix4x4 newViewMatrix = Camera->viewMatrix() * zoom;

     //The position of the mouse after the zoom
     QVector3D afterZoom = Camera->unProject(screenCoords, 0.1f, newViewMatrix, QMatrix4x4());

     //Adjust the scale matrix, so the image is in the same location
     QVector3D changeInPosition = afterZoom - beforeZoom;
     newViewMatrix.translate(changeInPosition);

     //Update the camera
     Camera->setViewMatrix(newViewMatrix);
 }
