/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwLazLayerModel.h"

//Qt includes
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QLoggingCategory>
#include <QPointer>
#include <QVariant>

//Our includes
#include "cwCavingRegion.h"
#include "cwError.h"
#include "cwErrorListModel.h"
#include "cwLazLoader.h"
#include "cwLocalProjection.h"
#include "cwProject.h"
#include "cwSaveLoad.h"

Q_LOGGING_CATEGORY(lcLazModel, "cw.laz.model")

namespace {
QString shortId(const cwLazLayer* layer) {
    return layer == nullptr
        ? QStringLiteral("(null)")
        : layer->id().toString(QUuid::WithoutBraces).left(8);
}
} // namespace

cwLazLayerModel::cwLazLayerModel(QObject* parent) :
    QAbstractListModel(parent)
{
}

cwLazLayerModel::~cwLazLayerModel() = default;

int cwLazLayerModel::rowCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    return m_layers.size();
}

QVariant cwLazLayerModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_layers.size()) {
        return QVariant();
    }
    cwLazLayer* layer = m_layers.at(index.row());
    switch (role) {
    case LayerRole:        return QVariant::fromValue<QObject*>(layer);
    case NameRole:         return layer->name();
    case SourcePathRole:   return layer->sourcePath();
    case SourceCSRole:     return layer->sourceCS();
    case SourceCSDisplayNameRole: return layer->sourceCSDisplayName();
    case PointSizeRole:    return layer->pointSize();
    case LoadStatusRole:   return QVariant::fromValue(layer->loadStatus());
    case PointCountRole:   return layer->pointCount();
    case EnabledRole:      return layer->enabled();
    default:               return QVariant();
    }
}

bool cwLazLayerModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_layers.size()) {
        return false;
    }
    cwLazLayer* layer = m_layers.at(index.row());
    switch (role) {
    case EnabledRole:
        layer->setEnabled(value.toBool());
        return true;
    default:
        return false;
    }
}

QHash<int, QByteArray> cwLazLayerModel::roleNames() const
{
    static const QHash<int, QByteArray> roles = {
        {LayerRole,        "lazLayer"},
        {NameRole,         "name"},
        {SourcePathRole,   "sourcePath"},
        {SourceCSRole,     "sourceCS"},
        {SourceCSDisplayNameRole, "sourceCSDisplayName"},
        {PointSizeRole,    "pointSize"},
        {LoadStatusRole,   "loadStatus"},
        {PointCountRole,   "pointCount"},
        {EnabledRole,      "enabled"}
    };
    return roles;
}

void cwLazLayerModel::addFromFiles(QList<QUrl> urls)
{
    if (urls.isEmpty()) {
        return;
    }
    auto* p = this->project();
    if (p == nullptr) {
        qWarning() << "cwLazLayerModel::addFromFiles: no parent project — drop";
        return;
    }
    if (m_gisLayersDir.absolutePath().isEmpty()) {
        qWarning() << "cwLazLayerModel::addFromFiles: GIS Layers dir not set —"
                   << "did setFileName run yet? Drop.";
        return;
    }

    // Nothing to seed from the incoming files here: the copy is followed by a
    // rescan, and that is where each new layer gets its frame — from its own
    // header, against the layer that will carry the anchor.
    QPointer<cwLazLayerModel> self(this);
    const QDir destinationDir = m_gisLayersDir;
    p->addFiles(urls, destinationDir,
                [self](QList<QString> /*relativePaths*/) {
        if (self) {
            self->rescan();
        }
    });
}

void cwLazLayerModel::removeAt(int index)
{
    if (index < 0 || index >= m_layers.size()) {
        return;
    }
    cwLazLayer* layer = m_layers.at(index);
    // Emit before beginRemoveRows so cwSaveLoad's slot resolves the .cwlaz
    // path through m_objectStates while the entry is still live; the
    // rowsAboutToBeRemoved handler drops the entry immediately after.
    // Rescan-driven removals deliberately skip this signal so an
    // externally-moved .laz doesn't take its paired .cwlaz with it.
    emit aboutToRemoveLayerByUser(layer);
    beginRemoveRows(QModelIndex(), index, index);
    m_layers.takeAt(index);
    endRemoveRows();
    layer->deleteLater();
    emit countChanged();
}

bool cwLazLayerModel::rename(int row, const QString& newBasename)
{
    // Route user-visible failures through the project's errorModel —
    // the same surface rescan() uses to report failed metadata loads.
    // Returns false so callers can branch on the result; the cwError is
    // already queued for UI display by the time we return.
    auto failWith = [this](const QString& message) {
        if (auto* p = project()) {
            p->errorModel()->append(cwError(message, cwError::Warning));
        }
        return false;
    };

    if (row < 0 || row >= m_layers.size()) {
        return failWith(tr("Row %1 is out of range.").arg(row));
    }

    // Validation: the new basename must be a bare filename (no path
    // separators, no extension), and non-empty after trimming. Rejecting
    // a dot anywhere is stricter than "no extension" but matches user
    // intent — renaming "alpha" to "alpha.v2" would silently change the
    // suffix the rescan pairs on.
    const QString trimmed = newBasename.trimmed();
    if (trimmed.isEmpty()) {
        return failWith(tr("New name must not be empty."));
    }
    if (trimmed.contains(QLatin1Char('/')) || trimmed.contains(QLatin1Char('\\'))) {
        return failWith(tr("New name must not contain path separators."));
    }
    if (trimmed.contains(QLatin1Char('.'))) {
        return failWith(tr("New name must not contain a file extension."));
    }

    cwLazLayer* layer = m_layers.at(row);
    const QString oldSourcePath = layer->sourcePath();
    const QFileInfo oldInfo(oldSourcePath);
    const QString suffix = oldInfo.suffix();
    if (suffix.isEmpty()) {
        // Without a real extension the new path would end in a trailing
        // dot, which Win32 silently strips at the filesystem layer and
        // which breaks rescan's basename pairing on every platform.
        // .laz / .las are the only extensions the loader accepts, so an
        // empty suffix means the input layer is already in a broken
        // state — refuse the rename rather than worsen it.
        return failWith(tr("Source file has no extension; cannot rename."));
    }
    const QDir layerDir = oldInfo.absoluteDir();
    const QString newSourcePath = layerDir.absoluteFilePath(trimmed + QLatin1Char('.') + suffix);
    const QString newCwlazPath = layerDir.absoluteFilePath(trimmed + QStringLiteral(".cwlaz"));

    // Exact-equality early-out: identical paths mean nothing to do. Note
    // the comparison is case-sensitive — case-only renames ("Foo" ->
    // "foo") are legitimate user operations that need to flow through
    // the rename pipeline so the on-disk basename matches the user's
    // intent, even on filesystems where the two names refer to the same
    // inode.
    if (oldSourcePath == newSourcePath) {
        return true;
    }

    // Collision check against other loaded layers (any extension match on
    // the new basename) — we cannot tolerate two layers with the same
    // basename in the same directory since cwLazLayerModel keys identity
    // on basename.
    for (int i = 0; i < m_layers.size(); ++i) {
        if (i == row) {
            continue;
        }
        const cwLazLayer* other = m_layers.at(i);
        const QFileInfo otherInfo(other->sourcePath());
        if (otherInfo.absoluteDir().absolutePath() != layerDir.absolutePath()) {
            continue;
        }
        if (otherInfo.completeBaseName().compare(trimmed, Qt::CaseInsensitive) == 0) {
            return failWith(tr("A layer named \"%1\" already exists.").arg(trimmed));
        }
    }

    // Collision check against stray files on disk: a .laz / .las / .cwlaz
    // sibling at the target basename would be overwritten / orphaned by
    // the Move job. Reject early; the user can delete the stray and try
    // again.
    //
    // The candidate-vs-oldSourcePath bypass MUST compare case-
    // insensitively: on macOS HFS+/APFS-CI and Windows NTFS,
    // QFileInfo::exists matches case-insensitively, so a layer at
    // "Foo.laz" being renamed to "foo" would find "foo.laz" existing on
    // disk (it's the same file as "Foo.laz"). Without the case-
    // insensitive compare, every case-only rename would be rejected.
    const QStringList strayCandidates = {
        layerDir.absoluteFilePath(trimmed + QStringLiteral(".laz")),
        layerDir.absoluteFilePath(trimmed + QStringLiteral(".las")),
        newCwlazPath
    };
    const QString oldCwlazPath = layerDir.absoluteFilePath(
                oldInfo.completeBaseName() + QStringLiteral(".cwlaz"));
    for (const QString& candidate : strayCandidates) {
        if (QString::compare(candidate, oldSourcePath, Qt::CaseInsensitive) == 0) {
            continue;
        }
        if (QString::compare(candidate, oldCwlazPath, Qt::CaseInsensitive) == 0) {
            continue;
        }
        if (QFileInfo::exists(candidate)) {
            return failWith(tr("A file named \"%1\" already exists in the GIS Layers directory.")
                            .arg(QFileInfo(candidate).fileName()));
        }
    }

    // Update the layer's in-memory path so signal listeners (cwSaveLoad)
    // see the new path when they compute the queued .cwlaz destination
    // via absolutePathPrivate(layer). renameSourcePath emits
    // sourcePathChanged + nameChanged synchronously; connectLayer's
    // per-property handlers already forward those to dataChanged for
    // SourcePathRole + NameRole, so no manual dataChanged emission is
    // needed here.
    layer->renameSourcePath(newSourcePath);

    emit layerRenamed(layer, oldSourcePath, newSourcePath);
    return true;
}

cwLazLayer* cwLazLayerModel::layerAt(int index) const
{
    if (index < 0 || index >= m_layers.size()) {
        return nullptr;
    }
    return m_layers.at(index);
}

void cwLazLayerModel::setFutureManagerToken(const cwFutureManagerToken& token)
{
    m_futureManagerToken = token;
    for (cwLazLayer* layer : std::as_const(m_layers)) {
        layer->setFutureManagerToken(token);
    }
}

void cwLazLayerModel::setRegionFrameCS(const QString& cs)
{
    if (m_regionFrameCS == cs) {
        return;
    }
    m_regionFrameCS = cs;
    for (cwLazLayer* layer : std::as_const(m_layers)) {
        layer->setRegionFrameCS(cs);
    }
}

void cwLazLayerModel::setGisLayersDir(const QDir& dir)
{
    if (m_gisLayersDir.absolutePath() == dir.absolutePath()) {
        return;
    }
    m_gisLayersDir = dir;
    // Defer the rescan to the event loop. During project load, this setter is
    // called from cwSaveLoad::setFileName, which runs *before* cwCavingRegion
    // ::setData restores the stored frame. Running rescan synchronously here
    // would derive a frame from the first layer's header only for the restore
    // to replace it — and reload every layer twice on the way. Queuing lets
    // setData run first, after which deriveFrameFromLaz is a no-op.
    QMetaObject::invokeMethod(this, &cwLazLayerModel::rescan, Qt::QueuedConnection);
}

void cwLazLayerModel::rescan()
{
    qCDebug(lcLazModel) << "rescan: BEGIN dir=" << m_gisLayersDir.absolutePath()
                        << "currentLayers=" << m_layers.size();
    const int initialCount = m_layers.size();

    QFileInfoList entries;
    if (!m_gisLayersDir.absolutePath().isEmpty() && m_gisLayersDir.exists()) {
        // Linux's filesystem is case-sensitive, so the upper-case patterns
        // matter; on macOS/Windows the filter is case-insensitive and the
        // duplication is harmless (entryInfoList dedupes by absolute path).
        const QStringList nameFilters{
            QStringLiteral("*.laz"),
            QStringLiteral("*.las"),
            QStringLiteral("*.LAZ"),
            QStringLiteral("*.LAS")
        };
        entries = m_gisLayersDir.entryInfoList(
                    nameFilters,
                    QDir::Files | QDir::NoDotAndDotDot,
                    QDir::Name);
    }

    // Diff existing layers against the directory listing instead of doing a
    // wholesale clear-and-rebuild. Preserves cwLazLayer objects whose
    // underlying file is still present *and* unchanged so their
    // already-loaded point data doesn't have to be re-streamed from disk
    // after each clip / add.

    struct FileFingerprint {
        qint64 size;
        QDateTime mtime;
    };
    QHash<QString, FileFingerprint> wantedFiles;
    wantedFiles.reserve(entries.size());
    for (const QFileInfo& entry : entries) {
        wantedFiles.insert(entry.absoluteFilePath(), {entry.size(), entry.lastModified()});
    }

    // Phase 1: remove layers whose file disappeared *or* whose (size, mtime)
    // pair no longer matches — that's an in-place overwrite, so the
    // in-memory point data is stale and the layer must be reloaded. Reverse
    // iteration keeps take-at indices stable across the in-place erase. The
    // paired .cwlaz is the source of truth for enabled/UUID, so a same-named
    // file reappearing in Phase 2 re-adopts the prior state from disk.
    for (int row = m_layers.size() - 1; row >= 0; --row) {
        cwLazLayer* layer = m_layers.at(row);
        const auto it = wantedFiles.constFind(layer->sourcePath());
        const bool missing = (it == wantedFiles.constEnd());
        const bool stale = !missing
                && (it.value().size != layer->sourceSize()
                    || it.value().mtime != layer->sourceMtime());
        if (missing || stale) {
            qCDebug(lcLazModel) << "rescan: remove" << shortId(layer)
                                << "file=" << QFileInfo(layer->sourcePath()).fileName()
                                << (stale ? "(stale fingerprint)" : "(file gone)");
            beginRemoveRows(QModelIndex(), row, row);
            m_layers.removeAt(row);
            endRemoveRows();
            layer->deleteLater();
        }
    }

    // Phase 2: walk the directory listing in order. For each entry, either
    // the matching existing layer is already at the current row (advance),
    // or it sits at a later row (move into place), or it's a new file
    // (create + insert).
    int row = 0;
    for (const QFileInfo& entry : entries) {
        const QString path = entry.absoluteFilePath();

        if (row < m_layers.size() && m_layers.at(row)->sourcePath() == path) {
            ++row;
            continue;
        }

        // Existing layer at a later row — only happens when the directory
        // sort order disagrees with the current model order (e.g. one file
        // removed and another inserted whose name sorts in between).
        int srcRow = -1;
        for (int k = row + 1; k < m_layers.size(); ++k) {
            if (m_layers.at(k)->sourcePath() == path) {
                srcRow = k;
                break;
            }
        }
        if (srcRow > row) {
            beginMoveRows(QModelIndex(), srcRow, srcRow, QModelIndex(), row);
            m_layers.move(srcRow, row);
            endMoveRows();
            ++row;
            continue;
        }

        // New file — create a layer and insert at the current row.
        cwLazLayer* layer = createLayer();

        // Apply persisted state from the paired <basename>.cwlaz first, so the
        // layer carries its final UUID before anything can anchor to it, and
        // before observers wired into rowsInserted (the scene node, keyword
        // filter pipeline, cwSaveLoad's enabledChanged hookup) see it. A layer
        // the file says is disabled also never starts the load setSourcePath
        // would otherwise kick off below.
        const QString metadataPath = m_gisLayersDir.absoluteFilePath(
                    QFileInfo(path).completeBaseName() + QStringLiteral(".cwlaz"));
        if (QFileInfo::exists(metadataPath)) {
            auto result = cwSaveLoad::loadLazLayer(metadataPath);
            if (result.hasError()) {
                // A corrupt .cwlaz cannot be silently replaced — the next
                // save would mint a fresh UUID and permanently lose the
                // persisted identity. Skip the layer; surface the failure
                // through cwErrorModel so the user can investigate.
                qCWarning(lcLazModel) << "rescan: skipping" << entry.fileName()
                                      << "— failed to load" << metadataPath
                                      << ":" << result.errorMessage();
                if (auto* p = project()) {
                    p->errorModel()->append(cwError(
                        tr("Cannot load LAZ layer %1: %2")
                            .arg(entry.fileName(), result.errorMessage()),
                        cwError::Warning));
                }
                delete layer;
                continue;
            }
            layer->setData(result.value());
        }
        // No paired .cwlaz: the layer takes its defaults (enabled = true,
        // a freshly-minted UUID). The next save flush writes the .cwlaz
        // alongside, making the on-disk pair the source of truth from
        // then on.

        // The load below has to have somewhere to load into, and on a project
        // with no fix stations this layer is the only thing that can say where
        // that is.
        deriveFrameFromLaz(path, layer);
        layer->setRegionFrameCS(m_regionFrameCS);

        // setSourcePath — which starts the load — before announcing the row:
        // it triggers updateNameKeyword/updateFileNameKeyword on the layer's
        // keyword model. Running it after beginInsertRows lets the scene node
        // register a keyword item carrying only Type+ObjectId, then the late
        // keyword changes shuttle the same item through both accepted and
        // rejected filter pipelines, thrashing cwKeywordVisibility
        // (last-inserted wins).
        layer->setSourcePath(path);

        qCDebug(lcLazModel) << "rescan: insert" << shortId(layer)
                            << "file=" << entry.fileName();
        beginInsertRows(QModelIndex(), row, row);
        m_layers.insert(row, layer);
        endInsertRows();
        ++row;
    }

    qCDebug(lcLazModel) << "rescan: END layers=" << m_layers.size();
    if (m_layers.size() != initialCount) {
        emit countChanged();
    }
}

void cwLazLayerModel::clear()
{
    if (m_layers.isEmpty()) {
        return;
    }
    qCDebug(lcLazModel) << "clear: destroying" << m_layers.size() << "layers";
    beginResetModel();
    qDeleteAll(m_layers);
    m_layers.clear();
    endResetModel();
    emit countChanged();
}

void cwLazLayerModel::connectLayer(cwLazLayer* layer)
{
    auto emitForRole = [this, layer](int role) {
        const int row = indexOf(layer);
        if (row < 0) return;
        const QModelIndex idx = index(row, 0);
        emit dataChanged(idx, idx, {role});
    };

    connect(layer, &cwLazLayer::nameChanged, this, [emitForRole]() { emitForRole(NameRole); });
    connect(layer, &cwLazLayer::sourcePathChanged, this, [emitForRole]() { emitForRole(SourcePathRole); });
    connect(layer, &cwLazLayer::sourceCSChanged, this, [emitForRole]() {
        emitForRole(SourceCSRole);
        emitForRole(SourceCSDisplayNameRole);
    });
    connect(layer, &cwLazLayer::pointSizeChanged, this, [emitForRole]() { emitForRole(PointSizeRole); });
    connect(layer, &cwLazLayer::loadStatusChanged, this, [emitForRole]() { emitForRole(LoadStatusRole); });
    connect(layer, &cwLazLayer::pointCountChanged, this, [emitForRole]() { emitForRole(PointCountRole); });
    connect(layer, &cwLazLayer::enabledChanged, this, [emitForRole]() { emitForRole(EnabledRole); });
}

cwLazLayer* cwLazLayerModel::createLayer()
{
    auto* layer = new cwLazLayer(this);
    connectLayer(layer);
    layer->setFutureManagerToken(m_futureManagerToken);
    return layer;
}

int cwLazLayerModel::indexOf(cwLazLayer* layer) const
{
    for (int i = 0; i < m_layers.size(); ++i) {
        if (m_layers.at(i) == layer) {
            return i;
        }
    }
    return -1;
}

void cwLazLayerModel::deriveFrameFromLaz(const QString& sourcePath, cwLazLayer* layer)
{
    auto* region = qobject_cast<cwCavingRegion*>(parent());
    if (region == nullptr || layer == nullptr) {
        return;
    }

    cwGeoReference* geoReference = region->geoReference();
    if (geoReference->hasCoordinateSystem()) {
        // The project already knows where it is — from its own fix stations,
        // from an earlier layer, or from the file it was loaded out of. Nothing
        // here may move that.
        return;
    }

    // The header alone, because the load this unblocks hasn't run yet: the
    // layer has no sourceCS or bounding box to read until it has.
    const auto probe = cwLazLoader::probeHeader(sourcePath);
    if (!probe.valid || probe.sourceCS.isEmpty()) {
        // A cloud with no CRS of its own places nothing. It still loads —
        // untransformed, in whatever units the file is in — exactly as it did
        // before there was a frame to load into.
        return;
    }

    const QString localCS = cwLocalProjection::deriveFrom(probe.sourceCS, probe.bboxCenter);
    if (localCS.isEmpty()) {
        return;
    }
    geoReference->anchorTo({cwGeoReference::Anchor::LazLayer, layer->id()}, localCS);
}

cwProject* cwLazLayerModel::project() const
{
    auto* region = qobject_cast<cwCavingRegion*>(parent());
    if (region == nullptr) {
        return nullptr;
    }
    return region->parentProject();
}

QString cwLazLayerModel::folderName()
{
    return QStringLiteral("GIS Layers");
}

