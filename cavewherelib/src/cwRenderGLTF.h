#ifndef CWRENDERGLTF_H
#define CWRENDERGLTF_H

//our includes
#include "cwRenderObject.h"
#include "cwGltfLoader.h"
#include "cwTracked.h"

//Qt include
#include <QQmlEngine>
#include <QtCore/qproperty.h>

class cwRenderGLTF : public cwRenderObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(RenderGLTF)

    Q_PROPERTY(QString gltfFilePath READ gltfFilePath WRITE setGLTFFilePath NOTIFY gltfFilePathChanged)

    friend class cwRHIGltf;

public:

    explicit cwRenderGLTF(QObject *parent = nullptr);

    // Accessor without "get" prefix (per your style)
    QString gltfFilePath() const { return m_gltfFilePath; }

public slots:
    // Setter to choose which glTF file to render
    void setGLTFFilePath(const QString &filePath);

    // Convenience overload for QML/URLs
    void setGLTFUrl(const QUrl &url);

    //This is for testing
    Q_INVOKABLE void setRotation(float x, float y, float z, float angle);
    Q_INVOKABLE void setTranslation(float x, float y, float z);

signals:
    void gltfFilePathChanged();

protected:
    cwRHIObject *createRHIObject() override;

private:
    void updateGeometryIntersector(const cw::gltf::SceneCPU& data);

    QString m_gltfFilePath;

    cw::gltf::SceneCPU m_data;
    bool m_dataChanged = false;

    cwTracked<QMatrix4x4> m_modelMatrix;

    QProperty<QVector4D> m_rotation;
    QProperty<QVector3D> m_translation;
    QProperty<QMatrix4x4> m_modelMatrixProperty;

    QPropertyNotifier m_modelMatrixUpdated;

    QList<cwGeometryItersecter::Key> m_matrixObjects;

    void updateModelMatrix();




};

#endif // CWRENDERGLTF_H
