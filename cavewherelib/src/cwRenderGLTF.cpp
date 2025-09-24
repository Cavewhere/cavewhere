//Our includes
#include "cwRenderGLTF.h"
#include "cwRHIGltf.h"


cwRenderGLTF::cwRenderGLTF(QObject *parent)
    : cwRenderObject{parent}
{}

void cwRenderGLTF::setGLTFFilePath(const QString &filePath)
{
    m_data.setValue(cw::gltf::Loader::loadGltf(filePath));
    update();
}

void cwRenderGLTF::setGLTFUrl(const QUrl &url)
{

}

cwRHIObject *cwRenderGLTF::createRHIObject()
{
    return new cwRHIGltf();
}
