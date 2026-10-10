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
#include "cwSurveyNode.h"

//Qt includes
#include <QDate>
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
    Q_PROPERTY(QList<cwScanRow> rows READ rows NOTIFY scanChanged FINAL)
    Q_PROPERTY(int surveyCount READ surveyCount NOTIFY scanChanged FINAL)
    Q_PROPERTY(int stationCount READ stationCount NOTIFY scanChanged FINAL)
    Q_PROPERTY(int missingFileCount READ missingFileCount NOTIFY scanChanged FINAL)
    Q_PROPERTY(int topLevelBlockCount READ topLevelBlockCount NOTIFY scanChanged FINAL)
    Q_PROPERTY(int rootStationCount READ rootStationCount NOTIFY scanChanged FINAL)
    Q_PROPERTY(QDate rootDate READ rootDate NOTIFY scanChanged FINAL)
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

    // The tree an attach builds, root first in pre-order. The dialog's
    // structure preview walks it directly.
    QList<cwScanRow> rows() const { return m_rows; }

    // Summary counts over rows(): the units with a file on disk, the distinct
    // cavern-relative station names they hold (a Compass link station is one
    // name per file until cavern equates them), and the listed files the disk
    // lacks.
    int surveyCount() const;
    int stationCount() const;
    int missingFileCount() const;

    // What a preview row reads as: its node kind for a node, "Trip" for a
    // unit-only row (the trip an attach derives from it).
    Q_INVOKABLE QString kindLabel(const cwScanRow& row) const;

    // What Replace with the scanned file costs \a root's derived trips (§6.11
    // case 5): a scrap loses an anchor when one of its stations is a name the
    // scanned file holds nowhere. The notes count is the notes holding at least
    // one such scrap. Both are zero until the scan is valid.
    Q_INVOKABLE int notesLosingAnchors(cwSurveyNode* root) const;
    Q_INVOKABLE int scrapsLosingAnchors(cwSurveyNode* root) const;

    // Counted from blocks() rather than stored, so the two can never
    // disagree. With entryHasOwnShots it drives the multi-cave
    // heuristic: several top-level blocks and no shots of the entry's
    // own means the file looks like a region master.
    int topLevelBlockCount() const;

    // The stations that live outside every block, which one
    // whole-cave trip windows. The dialog previews them as a
    // whole-file row above the block tree, so a Compass or
    // prefix-less Walls file explains its single trip up front.
    int rootStationCount() const { return m_rootStationCount; }

    // The date that whole-cave trip is seeded from; invalid when the
    // file writes none outside its blocks.
    QDate rootDate() const { return m_rootDate; }

    bool entryHasOwnShots() const { return m_entryHasOwnShots; }

    // The entry file's first-level includes, offered as alternate
    // attach targets when the heuristic fires.
    QStringList entryDirectIncludes() const { return m_entryDirectIncludes; }

signals:
    void sourcePathChanged();
    void scanningChanged();

    // One signal for the whole result payload (valid / errorMessage /
    // warnings / fileCount / blocks / rows / rootStationCount / rootDate /
    // entryHasOwnShots / entryDirectIncludes) - they only ever change
    // together.
    void scanChanged();

private:
    struct AnchorLoss
    {
        int notes = 0;
        int scraps = 0;
    };

    AnchorLoss anchorLoss(const cwSurveyNode* root) const;

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
    QList<cwScanRow> m_rows;
    int m_rootStationCount = 0;
    QDate m_rootDate;
    bool m_entryHasOwnShots = false;
    QStringList m_entryDirectIncludes;

    AsyncFuture::Restarter<Monad::Result<cwExternalCenterlineScanner::ScanResult>> m_restarter;
};

#endif // CWEXTERNALCENTERLINESCANPREVIEW_H
