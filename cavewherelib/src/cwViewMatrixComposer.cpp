//Our includes
#include "cwViewMatrixComposer.h"
#include "cwCamera.h"


cwViewMatrixComposer::cwViewMatrixComposer(QObject* parent)
    : QObject(parent)
{
}

void cwViewMatrixComposer::setCamera(cwCamera* camera)
{
    if (m_camera != camera)
    {
        m_camera = camera;
        compose();
        emit cameraChanged();
    }
}

void cwViewMatrixComposer::setBaseViewMatrix(const QMatrix4x4& matrix)
{
    m_baseViewMatrix = matrix;
    compose();
}

void cwViewMatrixComposer::setHeadOffset(const QMatrix4x4& offset)
{
    m_headOffset = offset;
    compose();
}

void cwViewMatrixComposer::compose()
{
    if (m_camera)
    {
        if (m_headOffset.isIdentity())
        {
            m_camera->setViewMatrix(m_baseViewMatrix);
        }
        else
        {
            m_camera->setViewMatrix(m_headOffset * m_baseViewMatrix);
        }
    }
}
