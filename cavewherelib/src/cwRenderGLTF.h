#ifndef CWRENDERGLTF_H
#define CWRENDERGLTF_H


//Monad includes
#include "Monad/Result.h"

//our includes
#include "asyncfuture.h"
#include "cwFutureManagerToken.h"
#include "cwRenderObject.h"
#include "cwGltfLoader.h"
#include "cwTracked.h"
#include "cwGeometryItersecter.h"
#include "cwRenderTexturedItems.h"
#include "CaveWhereLibExport.h"

//Qt include
#include <QQmlEngine>
#include <QtCore/qproperty.h>

class CAVEWHERE_LIB_EXPORT cwRenderGLTF : public cwRenderTexturedItems
{
    Q_OBJECT
    QML_NAMED_ELEMENT(RenderGLTF)

    Q_PROPERTY(QString gltfFilePath READ gltfFilePath WRITE setGLTFFilePath NOTIFY gltfFilePathChanged)
    Q_PROPERTY(cwFutureManagerToken futureManagerToken READ futureManagerToken WRITE setFutureManagerToken NOTIFY futureManagerTokenChanged FINAL)
    Q_PROPERTY(QMatrix4x4 modelMatrix READ modelMatrix WRITE setModelMatrix NOTIFY modelMatrixChanged BINDABLE bindableModelMatrix)
    Q_PROPERTY(Status status READ status NOTIFY statusChanged)

    friend class cwRHIGltf;

public:
    enum class Status {
        Null,
        Ready,
        Loading,
        Error
    };
    Q_ENUM(Status)


    explicit cwRenderGLTF(QObject *parent = nullptr);

    QString gltfFilePath() const { return m_gltfFilePath; }

    cwFutureManagerToken futureManagerToken() const;
    void setFutureManagerToken(const cwFutureManagerToken &newFutureManagerToken);

    QMatrix4x4 modelMatrix() const { return m_modelMatrixProperty.value(); }
    void setModelMatrix(const QMatrix4x4& matrix) { m_modelMatrixProperty = matrix; }
    QBindable<QMatrix4x4> bindableModelMatrix() { return &m_modelMatrixProperty; }

    Status status() const { return m_status.value(); }

    // void setGltf(const QFuture<Monad::Result<cw::gltf::SceneCPU>>& gltfFuture);

    Q_INVOKABLE QBox3D boundingBox() const;

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
    void modelMatrixChanged();

    void futureManagerTokenChanged();
    void statusChanged();

protected:
    cwRHIObject *createRHIObject() override;

private:
    struct Load {
        QVector<cwRenderTexturedItems::Item> items;
        // cw::gltf::SceneCPU scene;
        // QList<cwGeometryItersecter::Key> matrixObjects;
        // QList<cwGeometryItersecter::Object> intersecterObjects;
    };

    QFuture<void> handleLoadFuture(QFuture<Monad::Result<Load>> loadFuture);

    // static Load toIntersectors(cwRenderObject* renderObject,
    //                            const cw::gltf::SceneCPU& data,
    //                            const QMatrix4x4& modelMatrix);

    QString m_gltfFilePath;

    QVector<uint32_t> m_items;

    // cw::gltf::SceneCPU m_data;
    // bool m_dataChanged = false;

    cwTracked<QMatrix4x4> m_modelMatrix;

    QProperty<QVector4D> m_rotation;
    QProperty<QVector3D> m_translation;
    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(cwRenderGLTF, QMatrix4x4, m_modelMatrixProperty, QMatrix4x4(), &cwRenderGLTF::modelMatrixChanged);
    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(cwRenderGLTF, cwRenderGLTF::Status, m_status, Status::Null, &cwRenderGLTF::statusChanged);

    QPropertyNotifier m_modelMatrixUpdated;

    cwFutureManagerToken m_futureManagerToken;
    AsyncFuture::Restarter<void> m_loadRestarter;

    // QFuture<void> m_loadFuture;

    void updateModelMatrix();




};

#endif // CWRENDERGLTF_H
