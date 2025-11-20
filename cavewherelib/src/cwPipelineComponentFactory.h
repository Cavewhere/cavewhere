#ifndef CWPIPELINECOMPONENTFACTORY_H
#define CWPIPELINECOMPONENTFACTORY_H

#include <QObject>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QHash>
#include <QString>
#include <QUrl>

class cwPipelineComponentFactory : public QObject {
    Q_OBJECT

public:
    explicit cwPipelineComponentFactory(QObject* parent = nullptr);

    // Creates a QQmlComponent based on the given QObject.
    // Returns nullptr if no mapping is found.
    QQmlComponent* createComponent(QObject* object, QQmlEngine* engine);

    // Registers a custom QML URL mapping for a class using its QMetaObject.
    void registerComponent(const QMetaObject& metaObject, const QUrl& qmlUrl);

    // Optionally clears all mappings.
    void clearMappings();

private:
    // Initializes default mappings.
    void initializeDefaultMappings();

    // Engine used for QQmlComponent creation.
    QQmlEngine* m_engine;

    // Mapping from meta class names to QML file URLs.
    QHash<QString, QUrl> m_mapping;
};

#endif // CWPIPELINECOMPONENTFACTORY_H
