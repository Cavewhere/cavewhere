#ifndef CWGLTFSCENE_H
#define CWGLTFSCENE_H

// Qt
#include <QObject>

// Ours
#include "cwScene.h"
class cwRenderRadialGradient;
class cwRenderGLTF;
#include "cwGlobals.h"

class CAVEWHERE_LIB_EXPORT cwGltfScene : public cwScene
{
    Q_OBJECT
    QML_NAMED_ELEMENT(GltfScene)
    Q_PROPERTY(cwRenderGLTF* gltf READ gltf CONSTANT)

public:
    explicit cwGltfScene(QObject* parent = nullptr);
    ~cwGltfScene() override = default;

    cwRenderGLTF* gltf() const;

private:
    Q_DISABLE_COPY(cwGltfScene)

private:
    cwRenderRadialGradient* m_background;
    cwRenderGLTF* m_gltf;
};

#endif // CWGLTFSCENE_H
