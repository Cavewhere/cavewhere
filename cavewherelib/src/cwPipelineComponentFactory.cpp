
//Our includes
#include "cwPipelineComponentFactory.h"
#include "cwFileNameArtifact.h"
#include "cwTemporaryFileNameArtifact.h"

//Qt includes
#include <QQmlComponent>

cwPipelineComponentFactory::cwPipelineComponentFactory(QObject* parent)
    : QObject(parent)
{
    // Q_ASSERT(m_engine != nullptr);
    initializeDefaultMappings();
}

void cwPipelineComponentFactory::initializeDefaultMappings() {
    // Register default mappings using the new registerComponent signature.
    registerComponent(cwFileNameArtifact::staticMetaObject, QUrl("qrc:/qt/qml/cavewherelib/qml/FileNameArtifactNode.qml"));
    registerComponent(cwTemporaryFileNameArtifact::staticMetaObject, QUrl("qrc:/qt/qml/cavewherelib/qml/TemporaryFileNameArtifact.qml"));
}

QQmlComponent* cwPipelineComponentFactory::createComponent(QObject* object, QQmlEngine *engine) {
    Q_ASSERT(object != nullptr);

    const QString className = object->metaObject()->className();
    QUrl qmlUrl = m_mapping.value(className);

    // If no mapping is found, return nullptr.
    if (qmlUrl.isEmpty()) {
        return nullptr;
    }

    // Create the QQmlComponent; the factory (this) is its parent.
    QQmlComponent* component = new QQmlComponent(engine, qmlUrl, this);
    if (component->isError()) {
        // Print any errors using qDebug().
        const QList<QQmlError> errors = component->errors();
        for (const QQmlError &error : errors) {
            qDebug() << error.toString();
        }
        Q_ASSERT(false);
    }
    return component;
}

void cwPipelineComponentFactory::registerComponent(const QMetaObject& metaObject, const QUrl& qmlUrl) {
    m_mapping.insert(metaObject.className(), qmlUrl);
}

void cwPipelineComponentFactory::clearMappings() {
    m_mapping.clear();
}
