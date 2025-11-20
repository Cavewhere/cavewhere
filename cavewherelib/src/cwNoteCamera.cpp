#include "cwNoteCamera.h"

cwNoteCamera::cwNoteCamera(QObject *parent)
    : cwCamera{parent}
{}

void cwNoteCamera::setRotation(double degrees)
{
    if(m_rotation != degrees) {
        m_rotation = degrees;

        QMatrix4x4 rotationModelMatrix;
        rotationModelMatrix.translate(m_rotationCenter.x(), m_rotationCenter.y(), 0.0);
        rotationModelMatrix.rotate(-degrees, 0.0, 0.0, 1.0);
        rotationModelMatrix.translate(-m_rotationCenter.x(), -m_rotationCenter.y(), 0.0);
        m_rotationModelMatrix = rotationModelMatrix;

        resize(viewport().size());

        emit rotationChanged();
        emit modelMatrixChanged();
    }
}

/**
  This converts a mouse click into note coordinates

  The note coordinate are normalized from 0.0 to 1.0 for both the x and y.
  */
QPointF cwNoteCamera::mapQtViewportToNote(QPoint qtViewportCoordinate) const {
    QPoint glViewportPoint = mapToGLViewport(qtViewportCoordinate);
    QVector3D notePosition = unProject(glViewportPoint, 0.0, m_rotationModelMatrix);
    return notePosition.toPointF();
}

/**
 * @brief cwImageItem::mapNoteToQtViewport
 * @param mapNote - This converts a map point into a QtViewport coordinate
 * @return
 */
QPointF cwNoteCamera::mapNoteToQtViewport(QPointF mapNote) const
{
    QPointF glViewportPoint = project(QVector3D(mapNote.x(), mapNote.y(), 0.0), m_rotationModelMatrix);
    return glViewportPoint;
}

/**
  \brief This updates the rotation center for the image

  By default the rotation center is 0.5, 0.5, in normalize image coordinates

  This will update the rotation center such that the image will rotate around
  the center of the view.

  This is useful for animating the Image while it's rotating
  */
void cwNoteCamera::updateRotationCenter()
{
    const auto viewport = this->viewport();
    QPoint center(viewport.width() / 2.0, viewport.height() / 2.0);
    m_rotationCenter = unProject(center, 0.0).toPointF();
}

void cwNoteCamera::resize(const QSize& windowSize)
{
    QSize imageSize = m_image.originalSize();

    QTransform transformer;
    transformer.rotate(m_rotation);
    QRectF rotatedImageRect = transformer.mapRect(QRectF(QPointF(), imageSize));

    QSize scaledImageSize = rotatedImageRect.size().toSize();
    scaledImageSize.scale(windowSize, Qt::KeepAspectRatio);

    float changeInWidth = windowSize.width() / (float)scaledImageSize.width();
    float changeInHeight = windowSize.height() / (float)scaledImageSize.height();

    float widthProjection = imageSize.width() * changeInWidth;
    float heightProjection = imageSize.height() * changeInHeight;

    QPoint center(windowSize.width() / 2.0, windowSize.height() / 2.0);
    QVector3D oldViewCenter = unProject(center, 0.0);

    cwProjection orthognalProjection;
    orthognalProjection.setOrtho(0.0, widthProjection,
                                 0.0, heightProjection,
                                 -1.0, 1.0);
    setProjection(orthognalProjection);

    QVector3D newViewCenter = unProject(center, 0.0);
    QVector3D difference = newViewCenter - oldViewCenter;
    QMatrix4x4 viewMatrix = this->viewMatrix();
    viewMatrix.translate(difference);
    setViewMatrix(viewMatrix);

    setViewport(QRect(QPoint(0.0, 0.0), windowSize));
}
