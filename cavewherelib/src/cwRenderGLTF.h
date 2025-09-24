#ifndef CWRENDERGLTF_H
#define CWRENDERGLTF_H

//our includes
#include "cwRenderObject.h"
#include "cwGltfLoader.h"
#include "cwTracked.h"

class cwRenderGLTF : public cwRenderObject
{
    Q_OBJECT
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

signals:
    void gltfFilePathChanged();

protected:
    cwRHIObject *createRHIObject() override;

private:
    QString m_gltfFilePath;

    cwTracked<cw::gltf::SceneCPU> m_data;

};

#endif // CWRENDERGLTF_H
