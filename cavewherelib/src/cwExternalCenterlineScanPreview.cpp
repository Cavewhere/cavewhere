/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwExternalCenterlineScanPreview.h"
#include "cwConcurrent.h"
#include "cwTrip.h"
#include "cwSurveyNoteModel.h"
#include "cwNote.h"
#include "cwScrap.h"
#include "cwStation.h"

//Qt includes
#include <QSet>

//Std includes
#include <algorithm>

cwExternalCenterlineScanPreview::cwExternalCenterlineScanPreview(QObject* parent) :
    QObject(parent),
    m_restarter(this)
{
}

cwExternalCenterlineScanPreview::~cwExternalCenterlineScanPreview()
{
    // Cancel only, never drain (CLAUDE.md); the context guard drops
    // any in-flight apply.
    m_restarter.future().cancel();
}

void cwExternalCenterlineScanPreview::setSourcePath(const QString& sourcePath)
{
    if (m_sourcePath == sourcePath) {
        return;
    }
    m_sourcePath = sourcePath;
    emit sourcePathChanged();

    // The old path's result is stale the instant the path changes -
    // never show one path's summary against another path's field.
    applyResult(false, QString(), cwExternalCenterlineScanner::ScanResult());

    if (m_sourcePath.isEmpty()) {
        // Deliberately no restarter cancel here: cancelling the outer
        // future mid-burst leaves the queued start tracking a canceled
        // deferred, whose push-cancel would kill the NEXT scan started
        // in the same event-loop turn (stranding scanning == true). A
        // scan still in flight for the old path is dropped by the
        // path guard in the apply instead.
        setScanning(false);
        return;
    }

    setScanning(true);
    // Captured by value so a queued burst-start scans the path that
    // queued it - never a re-read of m_sourcePath, which may have been
    // cleared or replaced before the deferred start fires.
    const QString path = m_sourcePath;
    m_restarter.restart([this, path]() {
        auto future = cwConcurrent::run([path]() {
            return cwExternalCenterlineScanner::scan(path);
        });
        AsyncFuture::observe(future).context(this,
                [this, path](const Monad::Result<cwExternalCenterlineScanner::ScanResult>& result) {
            if (path != m_sourcePath) {
                // Superseded (or cleared) while the worker ran; a newer
                // scan or the cleared state owns the properties now.
                return;
            }
            setScanning(false);
            if (result.hasError()) {
                applyResult(false, result.errorMessage(),
                            cwExternalCenterlineScanner::ScanResult());
            } else {
                applyResult(true, QString(), result.value());
            }
        });
        return future;
    });
}

QString cwExternalCenterlineScanPreview::formatName() const
{
    return cwExternalCenterlineScanner::formatName(
        cwExternalCenterlineScanner::formatFor(m_sourcePath));
}

bool cwExternalCenterlineScanPreview::importSupported() const
{
    return cwExternalCenterlineScanner::formatFor(m_sourcePath)
            == cwExternalCenterlineScanner::Format::Survex;
}

void cwExternalCenterlineScanPreview::setScanning(bool scanning)
{
    if (m_scanning == scanning) {
        return;
    }
    m_scanning = scanning;
    emit scanningChanged();
}

int cwExternalCenterlineScanPreview::topLevelBlockCount() const
{
    int count = 0;
    for (const cwScanBlock& block : m_blocks) {
        if (block.depth == 0) {
            ++count;
        }
    }
    return count;
}

int cwExternalCenterlineScanPreview::notesLosingAnchors(cwSurveyNode* root) const
{
    return anchorLoss(root).notes;
}

int cwExternalCenterlineScanPreview::scrapsLosingAnchors(cwSurveyNode* root) const
{
    return anchorLoss(root).scraps;
}

cwExternalCenterlineScanPreview::AnchorLoss
cwExternalCenterlineScanPreview::anchorLoss(const cwSurveyNode* root) const
{
    AnchorLoss loss;
    if (root == nullptr || !m_valid) {
        return loss;
    }

    // A scrap resolves through the source root's whole view (§6.11 case 3), so
    // a station anywhere in the new file still anchors it.
    QSet<QString> scannedStations;
    for (const cwScanRow& row : m_rows) {
        for (const QString& station : row.stations) {
            scannedStations.insert(cwStation::canonicalKey(station));
        }
    }
    const auto isMissingFromScan = [&scannedStations](const cwNoteStation& station) {
        return !scannedStations.contains(cwStation::canonicalKey(station.name()));
    };

    const QList<cwTrip*> trips = root->allTrips();
    for (const cwTrip* trip : trips) {
        if (!trip->isDerived() || trip->parentNode()->sourceRoot() != root) {
            continue;
        }
        const QList<cwNote*> notes = trip->notes()->notes();
        for (const cwNote* note : notes) {
            const QList<cwScrap*> scraps = note->scraps();
            const int losing = static_cast<int>(std::count_if(scraps.cbegin(), scraps.cend(),
                                                              [&isMissingFromScan](const cwScrap* scrap) {
                const QList<cwNoteStation>& stations = scrap->stations();
                return std::any_of(stations.cbegin(), stations.cend(), isMissingFromScan);
            }));
            if (losing > 0) {
                ++loss.notes;
                loss.scraps += losing;
            }
        }
    }
    return loss;
}

void cwExternalCenterlineScanPreview::applyResult(
        bool valid, const QString& errorMessage,
        const cwExternalCenterlineScanner::ScanResult& scan)
{
    const int fileCount = static_cast<int>(scan.dependencies.size());
    if (m_valid == valid && m_errorMessage == errorMessage
        && m_warnings == scan.warnings && m_fileCount == fileCount
        && m_blocks == scan.blocks
        && m_rows == scan.rows
        && m_rootStationCount == scan.rootStationCount
        && m_rootDate == scan.rootDate
        && m_entryHasOwnShots == scan.entryHasOwnShots
        && m_entryDirectIncludes == scan.entryDirectIncludes) {
        return;
    }
    m_valid = valid;
    m_errorMessage = errorMessage;
    m_warnings = scan.warnings;
    m_fileCount = fileCount;
    m_blocks = scan.blocks;
    m_rows = scan.rows;
    m_rootStationCount = scan.rootStationCount;
    m_rootDate = scan.rootDate;
    m_entryHasOwnShots = scan.entryHasOwnShots;
    m_entryDirectIncludes = scan.entryDirectIncludes;
    emit scanChanged();
}
