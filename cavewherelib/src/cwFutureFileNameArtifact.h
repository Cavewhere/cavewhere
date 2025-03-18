#ifndef CWFUTUREFILENAMEARTIFACT_H
#define CWFUTUREFILENAMEARTIFACT_H

#include "cwArtifact.h"
#include <QObject>
#include <QtCore/qfuture.h>

#include <Monad/Result.h>

/**
 * @brief The cwFutureFileNameArtifact class represents a file-based artifact.
 *
 * It adds a bindable filename property that can be observed for changes.
 */
class cwFutureFileNameArtifact : public cwArtifact
{
    Q_OBJECT
    Q_PROPERTY(QFuture<Monad::ResultString> filename READ filename WRITE setFilename NOTIFY filenameChanged)

public:
    explicit cwFutureFileNameArtifact(QObject *parent = nullptr)
        : cwArtifact(parent)
    {
    }

    virtual ~cwFutureFileNameArtifact() {}

    QFuture<Monad::ResultString> filename() const { return m_filename; }
    void setFilename(const QFuture<Monad::ResultString> &newFilename);

signals:
    void filenameChanged();

private:

    QFuture<Monad::ResultString> m_filename;
};


#endif // CWFUTUREFILENAMEARTIFACT_H
