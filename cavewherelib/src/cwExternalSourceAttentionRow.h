/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWEXTERNALSOURCEATTENTIONROW_H
#define CWEXTERNALSOURCEATTENTIONROW_H

//Qt includes
#include <QList>
#include <QMetaType>
#include <QQmlEngine>
#include <QString>

//Our includes
#include "cwExternalSourceStatusModel.h"
#include "cwGlobals.h"

/**
 * One row of cwExternalCenterlineManager::sourcesNeedingAttention(): an
 * attachment whose source is Changed or SourceMissing, joining the owner's
 * display identity (from cwAttachedCenterlinesModel) onto the status
 * model's path, status, and source revision. Consumed by the source-change
 * banner's review list (ExternalSourceChangeHost.qml).
 *
 * ownerKey is the owner UUID spelled for JavaScript
 * (QUuid::WithoutBraces), which has no QUuid to compare or to key a map
 * with; it parses back into a QUuid on its way into updateFromSource.
 */
class CAVEWHERE_LIB_EXPORT cwExternalSourceAttentionRow
{
    Q_GADGET
    QML_VALUE_TYPE(cwExternalSourceAttentionRow)

    Q_PROPERTY(QString ownerKey READ ownerKey FINAL)
    Q_PROPERTY(QString ownerName READ ownerName FINAL)
    Q_PROPERTY(QString ownerKind READ ownerKind FINAL)
    Q_PROPERTY(QString caveName READ caveName FINAL)
    Q_PROPERTY(QString sourcePath READ sourcePath FINAL)
    Q_PROPERTY(QString sourceRevision READ sourceRevision FINAL)
    Q_PROPERTY(cwExternalSourceStatusModel::Status status READ status FINAL)

public:
    cwExternalSourceAttentionRow() = default;
    cwExternalSourceAttentionRow(const QString& ownerKey,
                                 const QString& ownerName,
                                 const QString& ownerKind,
                                 const QString& caveName,
                                 const QString& sourcePath,
                                 const QString& sourceRevision,
                                 cwExternalSourceStatusModel::Status status)
        : m_ownerKey(ownerKey)
        , m_ownerName(ownerName)
        , m_ownerKind(ownerKind)
        , m_caveName(caveName)
        , m_sourcePath(sourcePath)
        , m_sourceRevision(sourceRevision)
        , m_status(status)
    {
    }

    const QString& ownerKey() const { return m_ownerKey; }
    const QString& ownerName() const { return m_ownerName; }
    const QString& ownerKind() const { return m_ownerKind; }
    const QString& caveName() const { return m_caveName; }
    const QString& sourcePath() const { return m_sourcePath; }
    const QString& sourceRevision() const { return m_sourceRevision; }
    cwExternalSourceStatusModel::Status status() const { return m_status; }

private:
    QString m_ownerKey;
    QString m_ownerName;
    QString m_ownerKind;
    QString m_caveName;
    QString m_sourcePath;
    QString m_sourceRevision;
    cwExternalSourceStatusModel::Status m_status =
        cwExternalSourceStatusModel::Status::UpToDate;
};

//The review list walks the whole result in qml, so the list itself has to
//be a qml sequence. See cwStationHandleListRegistration for the same pattern.
class cwExternalSourceAttentionRowListRegistration
{
    Q_GADGET
    QML_FOREIGN(QList<cwExternalSourceAttentionRow>)
    QML_ANONYMOUS
    QML_SEQUENTIAL_CONTAINER(cwExternalSourceAttentionRow)
};

Q_DECLARE_METATYPE(cwExternalSourceAttentionRow)

#endif // CWEXTERNALSOURCEATTENTIONROW_H
