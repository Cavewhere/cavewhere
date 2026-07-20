/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWEXTERNALCENTERLINEREPORT_H
#define CWEXTERNALCENTERLINEREPORT_H

//Our includes
#include "cwGlobals.h"

//Qt includes
#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QUuid>
#include <QtQml/qqmlregistration.h>

/**
 * Value-type completion report carried by
 * cwExternalCenterlineManager::attachCompleted / detachCompleted, so
 * QML observes attach/detach completion through plain signals instead
 * of the underlying QFuture (commit-11 bridge decision,
 * plans/EXTERNAL_FILE_PHASE2.html section 11).
 *
 * One of three shapes, distinguished by the flags (not by field
 * patterns - a default-constructed report matches none of them):
 * success, canceled, or failure (!success && !canceled, with a
 * non-empty errorMessage). A canceled attach is a user action, not an
 * error, so dialogs stay silent on it. warnings and entryFile are
 * only populated on a successful attach.
 */
class CAVEWHERE_LIB_EXPORT cwExternalCenterlineReport
{
    Q_GADGET
    QML_VALUE_TYPE(cwExternalCenterlineReport)
    Q_PROPERTY(QUuid ownerId READ ownerId FINAL)
    Q_PROPERTY(bool success READ success FINAL)
    Q_PROPERTY(bool canceled READ canceled FINAL)
    Q_PROPERTY(QString errorMessage READ errorMessage FINAL)
    Q_PROPERTY(QStringList warnings READ warnings FINAL)
    Q_PROPERTY(QString entryFile READ entryFile FINAL)

public:
    cwExternalCenterlineReport() = default;

    static cwExternalCenterlineReport succeeded(const QUuid& ownerId,
                                                const QString& entryFile = QString(),
                                                const QStringList& warnings = QStringList())
    {
        cwExternalCenterlineReport report(ownerId);
        report.m_success = true;
        report.m_entryFile = entryFile;
        report.m_warnings = warnings;
        return report;
    }

    static cwExternalCenterlineReport failed(const QUuid& ownerId,
                                             const QString& errorMessage)
    {
        cwExternalCenterlineReport report(ownerId);
        report.m_errorMessage = errorMessage;
        return report;
    }

    static cwExternalCenterlineReport wasCanceled(const QUuid& ownerId)
    {
        cwExternalCenterlineReport report(ownerId);
        report.m_canceled = true;
        return report;
    }

    QUuid ownerId() const { return m_ownerId; }
    bool success() const { return m_success; }
    bool canceled() const { return m_canceled; }
    QString errorMessage() const { return m_errorMessage; }
    QStringList warnings() const { return m_warnings; }
    QString entryFile() const { return m_entryFile; }

private:
    explicit cwExternalCenterlineReport(const QUuid& ownerId)
        : m_ownerId(ownerId)
    {
    }

    QUuid m_ownerId;
    bool m_success = false;
    bool m_canceled = false;
    QString m_errorMessage;
    QStringList m_warnings;
    QString m_entryFile;
};

Q_DECLARE_METATYPE(cwExternalCenterlineReport)

#endif // CWEXTERNALCENTERLINEREPORT_H
