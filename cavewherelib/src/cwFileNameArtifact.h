#ifndef CWFILEARTIFACT_H
#define CWFILEARTIFACT_H

#include "cwArtifact.h"
#include <QObject>
#include <QObjectBindableProperty>

/**
 * @brief The cwFileNameArtifact class represents a file-based artifact.
 *
 * It adds a bindable filename property that can be observed for changes.
 */
class cwFileNameArtifact : public cwArtifact
{
    Q_OBJECT
    Q_PROPERTY(QString filename READ filename WRITE setFilename NOTIFY filenameChanged BINDABLE bindableFilename)

public:
    explicit cwFileNameArtifact(QObject *parent = nullptr)
        : cwArtifact(parent)
    {
    }

    virtual ~cwFileNameArtifact() {}

    QString filename() const { return m_filename; }
    void setFilename(const QString &filename) { m_filename = filename; }
    QBindable<QString> bindableFilename() { return QBindable<QString>(&m_filename); }

signals:
    void filenameChanged(const QString &filename);

private:
    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(cwFileNameArtifact, QString, m_filename, QString(), &cwFileNameArtifact::filenameChanged)
};

#endif // CWFILEARTIFACT_H
