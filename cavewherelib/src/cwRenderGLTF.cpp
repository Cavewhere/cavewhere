//Our includes
#include "cwRenderGLTF.h"
#include "cwRHIGltf.h"


cwRenderGLTF::cwRenderGLTF(QObject *parent)
    : cwRenderObject{parent}
{
    m_modelMatrixProperty.setBinding([this]() {
        QMatrix4x4 matrix;

        //TODO: make this user define
        //Default rotation for up
        matrix.rotate(90.0, 1.0, 0.0, 0.0);

        matrix.translate(m_translation);
        auto rotation = m_rotation.value();
        matrix.rotate(rotation.w(), rotation.x(), rotation.y(), rotation.z());
        return matrix;
    });

    m_modelMatrix.setValue(m_modelMatrixProperty.value());

}

void cwRenderGLTF::setGLTFFilePath(const QString &filePath)
{
    m_data = cw::gltf::Loader::loadGltf(filePath);
    m_dataChanged = true;
    update();
}

void cwRenderGLTF::setGLTFUrl(const QUrl &url)
{

}

void cwRenderGLTF::setRotation(float x, float y, float z, float angle)
{
    m_rotation = QVector4D(x, y, z, angle);
    m_modelMatrix.setValue(m_modelMatrixProperty.value());
    update();
}

void cwRenderGLTF::setTranslation(float x, float y, float z)
{
    m_translation = QVector3D(x, y, z);
    m_modelMatrix.setValue(m_modelMatrixProperty.value());
    update();
}

cwRHIObject *cwRenderGLTF::createRHIObject()
{
    return new cwRHIGltf();
}
