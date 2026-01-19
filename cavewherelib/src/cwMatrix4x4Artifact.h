// cwMatrix4x4Artifact.h
#ifndef CWMATRIX4X4ARTIFACT_H
#define CWMATRIX4X4ARTIFACT_H

#include "cwArtifact.h"
#include "CaveWhereLibExport.h"
#include <QObject>
#include <QObjectBindableProperty>
#include <QQmlEngine>
#include <QMatrix4x4>

class CAVEWHERE_LIB_EXPORT cwMatrix4x4Artifact : public cwArtifact {
    Q_OBJECT
    QML_NAMED_ELEMENT(Matrix4x4Artifact)
    Q_PROPERTY(QMatrix4x4 matrix4x4 READ matrix4x4 WRITE setMatrix4x4 NOTIFY matrix4x4Changed BINDABLE bindableMatrix4x4)

public:
    explicit cwMatrix4x4Artifact(QObject* parent = nullptr) : cwArtifact(parent) { setName("Matrix4x4"); }

    QMatrix4x4 matrix4x4() const { return m_matrix4x4; }
    void setMatrix4x4(const QMatrix4x4& matrix) { m_matrix4x4 = matrix; }
    QBindable<QMatrix4x4> bindableMatrix4x4() { return QBindable<QMatrix4x4>(&m_matrix4x4); }

signals:
    void matrix4x4Changed(const QMatrix4x4& matrix);

private:
    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(cwMatrix4x4Artifact, QMatrix4x4, m_matrix4x4, QMatrix4x4(), &cwMatrix4x4Artifact::matrix4x4Changed)
};

#endif // CWMATRIX4X4ARTIFACT_H
