/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWEXTERNALCENTERLINESCANPREVIEW_H
#define CWEXTERNALCENTERLINESCANPREVIEW_H

//Our includes
#include "cwExternalCenterlineScanner.h"
#include "cwGlobals.h"

//Qt includes
#include <QList>
#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QStringList>

//Async includes
#include <asyncfuture.h>

/**
 * Pre-attach scan preview for AttachExternalCenterlineDialog: point
 * sourcePath at a candidate entry file and bind the result
 * properties. Each path change clears the previous result and
 * supersedes any scan still in flight (AsyncFuture::Restarter), so
 * only the newest path's result ever lands.
 *
 * valid means the scan completed Ok for the current path - the
 * dialog gates [Attach] on it. warnings are advisory (missing
 * *include targets and the like; the attach may proceed);
 * errorMessage is a hard failure (missing file, unknown extension).
 */
class CAVEWHERE_LIB_EXPORT cwExternalCenterlineScanPreview : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(ExternalCenterlineScanPreview)

    Q_PROPERTY(QString sourcePath READ sourcePath WRITE setSourcePath NOTIFY sourcePathChanged FINAL)
    Q_PROPERTY(QString formatName READ formatName NOTIFY sourcePathChanged FINAL)
    Q_PROPERTY(bool importSupported READ importSupported NOTIFY sourcePathChanged FINAL)
    Q_PROPERTY(bool scanning READ scanning NOTIFY scanningChanged FINAL)
    Q_PROPERTY(bool valid READ valid NOTIFY scanChanged FINAL)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY scanChanged FINAL)
    Q_PROPERTY(QStringList warnings READ warnings NOTIFY scanChanged FINAL)
    Q_PROPERTY(int fileCount READ fileCount NOTIFY scanChanged FINAL)
    Q_PROPERTY(QList<cwScanBlock> blocks READ blocks NOTIFY scanChanged FINAL)
    Q_PROPERTY(int topLevelBlockCount READ topLevelBlockCount NOTIFY scanChanged FINAL)
    Q_PROPERTY(bool entryHasOwnShots READ entryHasOwnShots NOTIFY scanChanged FINAL)
    Q_PROPERTY(QStringList entryDirectIncludes READ entryDirectIncludes NOTIFY scanChanged FINAL)

public:
    explicit cwExternalCenterlineScanPreview(QObject* parent = nullptr);
    ~cwExternalCenterlineScanPreview();

    QString sourcePath() const { return m_sourcePath; }
    void setSourcePath(const QString& sourcePath);

    // Derived synchronously from the path's extension alone, so the
    // dialog's "Format: <name> (auto-detected)" line updates before
    // the scan lands.
    QString formatName() const;

    // Whether the copy-import path (cwSurveyImportManager) handles the
    // detected format. One home for the Survex-only rule so UI gates
    // can't drift from actual importer support.
    bool importSupported() const;

    bool scanning() const { return m_scanning; }
    bool valid() const { return m_valid; }
    QString errorMessage() const { return m_errorMessage; }
    QStringList warnings() const { return m_warnings; }
    int fileCount() const { return m_fileCount; }

    // Every named survey block in the closure, parents before
    // children. The dialog's structure preview walks it directly.
    QList<cwScanBlock> blocks() const { return m_blocks; }

    // Counted from blocks() rather than stored, so the two can never
    // disagree. With entryHasOwnShots it drives the multi-cave
    // heuristic: several top-level blocks and no shots of the entry's
    // own means the file looks like a region master.
    int topLevelBlockCount() const;

    bool entryHasOwnShots() const { return m_entryHasOwnShots; }

    // The entry file's first-level includes, offered as alternate
    // attach targets when the heuristic fires.
    QStringList entryDirectIncludes() const { return m_entryDirectIncludes; }

signals:
    void sourcePathChanged();
    void scanningChanged();

    // One signal for the whole result payload (valid / errorMessage /
    // warnings / fileCount / blocks / entryHasOwnShots /
    // entryDirectIncludes) - they only ever change together.
    void scanChanged();

private:
    void setScanning(bool scanning);
    // Takes the whole scan so a cleared or failed path applies a
    // default-constructed ScanResult and wipes every derived field at
    // once - a stale block tree must never show against a new path.
    void applyResult(bool valid, const QString& errorMessage,
                     const cwExternalCenterlineScanner::ScanResult& scan);

    QString m_sourcePath;
    bool m_scanning = false;
    bool m_valid = false;
    QString m_errorMessage;
    QStringList m_warnings;
    int m_fileCount = 0;
    QList<cwScanBlock> m_blocks;
    bool m_entryHasOwnShots = false;
    QStringList m_entryDirectIncludes;

    AsyncFuture::Restarter<Monad::Result<cwExternalCenterlineScanner::ScanResult>> m_restarter;
};

#endif // CWEXTERNALCENTERLINESCANPREVIEW_H
