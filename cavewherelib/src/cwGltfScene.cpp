//Our includes
#include "cwGltfScene.h"
#include "cwRenderRadialGradient.h"
#include "cwRenderGLTF.h"

cwGltfScene::cwGltfScene(QObject* parent)
    : cwScene(parent)
{
    m_background = new cwRenderRadialGradient();

    m_gltf = new cwRenderGLTF(this);
    addItem(m_background);
    addItem(m_gltf);
}

cwRenderGLTF* cwGltfScene::gltf() const
{
    return m_gltf;
}
