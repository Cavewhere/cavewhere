/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwSymbologyPaletteManager.h"
#include "cwSymbologyPalette.h"
#include "cwSymbologyPaletteIO.h"
#include "cwError.h"
#include "cwErrorModel.h"
#include "cwErrorListModel.h"
#include "cwRegionIOTask.h"

//Qt includes
#include <QCoreApplication>
#include <QDir>
#include <QHash>
#include <QSet>
#include <QUuid>
#include <QVector>

namespace {

// Turn a human palette name into a path-safe kebab-case directory stem. The
// directory name is cosmetic — a palette's identity is its UUID — so an empty
// or all-punctuation name still yields a usable folder.
QString slugify(const QString &name)
{
    QString slug;
    slug.reserve(name.size());
    bool lastWasDash = false;
    for (const QChar c : name) {
        if (c.isLetterOrNumber()) {
            slug.append(c.toLower());
            lastWasDash = false;
        } else if (!slug.isEmpty() && !lastWasDash) {
            slug.append(QLatin1Char('-'));
            lastWasDash = true;
        }
    }
    while (slug.endsWith(QLatin1Char('-'))) {
        slug.chop(1);
    }
    if (slug.isEmpty()) {
        slug = QStringLiteral("palette");
    }
    return slug;
}

// First "<slug>", "<slug>-2", "<slug>-3"… that is neither an existing
// subdirectory of root nor already claimed by a not-yet-flushed palette. A fork
// no longer writes its directory synchronously (cwSaveLoad's async full-write
// creates it), so on-disk existence alone can't prevent two rapid forks from
// colliding on a name — the taken set carries the in-memory claims too.
QString uniqueSubdirName(const QDir &root, const QString &slug, const QSet<QString> &taken)
{
    const auto isClaimed = [&](const QString &name) {
        return root.exists(name) || taken.contains(name);
    };
    QString candidate = slug;
    int suffix = 2;
    while (isClaimed(candidate)) {
        candidate = QStringLiteral("%1-%2").arg(slug).arg(suffix++);
    }
    return candidate;
}

// First "<base>", "<base>-2", "<base>-3"… not already in `taken`. Used to derive
// a collision-free glyph name when duplicating; mirrors uniqueSubdirName's suffix
// scheme but tests an in-palette name set rather than on-disk directories.
QString uniqueGlyphName(const QSet<QString> &taken, const QString &base)
{
    QString candidate = base;
    int suffix = 2;
    while (taken.contains(candidate)) {
        candidate = QStringLiteral("%1-%2").arg(base).arg(suffix++);
    }
    return candidate;
}

}

cwSymbologyPaletteManager *cwSymbologyPaletteManager::Singleton = nullptr;

cwSymbologyPaletteManager::cwSymbologyPaletteManager(QObject *parent) :
    QObject(parent),
    m_errorModel(new cwErrorModel(this))
{
    reload();
}

void cwSymbologyPaletteManager::initialize()
{
    if (Singleton == nullptr) {
        Singleton = new cwSymbologyPaletteManager(QCoreApplication::instance());
    }
}

cwSymbologyPaletteManager *cwSymbologyPaletteManager::instance()
{
    return Singleton;
}

QString cwSymbologyPaletteManager::folderName()
{
    return QStringLiteral("palettes");
}

void cwSymbologyPaletteManager::setPaletteDirectory(const QString &directory)
{
    if (m_paletteDirectory == directory) { return; }
    m_paletteDirectory = directory;
    reload();
}

void cwSymbologyPaletteManager::reload()
{
    // reload() is load-only: every palette it produces comes from disk (the
    // shipped default, then a directory scan), so it must never write back.
    // aboutToReload lets cwSaveLoad drop its per-palette auto-save wiring before
    // the reconciling setData() below runs — otherwise a survivor's setData
    // would re-enter the write path and rewrite just-loaded content. cwSaveLoad
    // also keys off this to adopt the loaded palettes as already-persisted
    // rather than enqueuing fresh writes.
    emit aboutToReload();

    m_errorModel->errors()->clear();

    // Phase 1 — scan every palette directory into an ordered list (default first).
    // A palette's identity is its id; the scan also carries the directory it came
    // from and whether it is writable here.
    struct ScannedPalette {
        cwSymbologyPaletteData data;
        QString directory;
        bool writable = false;
        bool isDefault = false;
    };

    QVector<ScannedPalette> scanned;
    QHash<QUuid, QString> directoryById;

    // The default palette is shipped embedded as a qrc resource and loaded
    // through the normal IO path. It is always present, read-only, and wins its
    // id on duplicates. A load failure here is a build defect (corrupt embedded
    // resource): report it and leave the default absent — there is no code-built
    // fallback.
    const QString defaultPath = QStringLiteral(":/palettes/cavewhere-default");
    const auto defaultResult = cwSymbologyPaletteIO::load(defaultPath);
    if (defaultResult.hasError()) {
        reportLoadProblem(
            QStringLiteral("failed to load the built-in default palette \"%1\": %2")
                .arg(defaultPath, defaultResult.errorMessage()));
    } else {
        const auto &loadResult = defaultResult.value();
        scanned.append({loadResult.palette, defaultPath, false, true});
        directoryById.insert(loadResult.palette.id, defaultPath);

        for (const cwError &warning : std::as_const(loadResult.warnings)) {
            reportLoadProblem(warning);
        }
    }

    // An empty directory means no project is open; serve only the shipped
    // default. (QDir("") resolves to the current working directory, so the
    // scan below must be skipped explicitly rather than relying on exists().)
    const QDir root(m_paletteDirectory);
    if (!m_paletteDirectory.isEmpty() && root.exists()) {
        const QFileInfoList entries =
            root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const QFileInfo &entry : entries) {
            const auto result = cwSymbologyPaletteIO::load(entry.absoluteFilePath());
            if (result.hasError()) {
                reportLoadProblem(result.errorMessage());
                continue;
            }
            const auto loadResult = result.value();
            const cwSymbologyPaletteData &data = loadResult.palette;

            const auto existing = directoryById.constFind(data.id);
            if (existing != directoryById.constEnd()) {
                reportLoadProblem(
                    QStringLiteral("duplicate palette id %1 in \"%2\"; keeping the one from \"%3\"")
                        .arg(data.id.toString(QUuid::WithoutBraces),
                             entry.absoluteFilePath(),
                             existing.value()));
                continue;
            }

            // A palette stamped newer than this build is forward-incompatible:
            // load it (its known brushes stay usable) but mark it read-only and
            // warn, so re-saving can't drop data the newer version added.
            const bool versionSupported =
                cwRegionIOTask::isVersionSupported(loadResult.maxFileVersion);
            if (!versionSupported) {
                reportLoadProblem(cwRegionIOTask::newerVersionWarning(
                    QStringLiteral("Symbology palette \"%1\"").arg(entry.fileName()),
                    loadResult.maxFileVersion,
                    QStringLiteral("It is read-only here. Please upgrade CaveWhere to edit it.")));
            }

            scanned.append({data, entry.absoluteFilePath(), versionSupported, false});
            directoryById.insert(data.id, entry.absoluteFilePath());

            // Rule-stack warnings for an accepted palette (commit 4.4). Fatal
            // problems already failed the load above, so these are all warnings.
            for (const cwError &warning : std::as_const(loadResult.warnings)) {
                reportLoadProblem(warning);
            }
        }
    }

    // Phase 2 — reconcile the scan against the live palettes by id, so a palette's
    // cwSymbologyPalette* (and any QML binding to it) survives a reload as long as
    // its id is still on disk: survivors are updated in place, only added palettes
    // are constructed, and only removed ones are deleted. take() pulls each reused
    // palette out of liveById, so whatever remains afterward is exactly the set
    // whose directory is gone.
    QHash<QUuid, cwSymbologyPalette *> liveById;
    liveById.reserve(m_palettes.size());
    for (auto *palette : std::as_const(m_palettes)) {
        liveById.insert(palette->id(), palette);
    }

    QList<cwSymbologyPalette *> rebuilt;
    rebuilt.reserve(scanned.size());
    m_default = nullptr;

    for (const ScannedPalette &entry : std::as_const(scanned)) {
        cwSymbologyPalette *palette = liveById.take(entry.data.id);
        if (palette == nullptr) {
            palette = new cwSymbologyPalette(this);
        }
        palette->setData(entry.data);
        palette->setWritable(entry.writable);
        palette->setDirectory(entry.directory);
        rebuilt.append(palette);

        if (entry.isDefault) {
            m_default = palette;
        }
    }

    for (auto *palette : std::as_const(liveById)) {
        delete palette;
    }

    m_palettes = rebuilt;

    emit palettesChanged();
}

void cwSymbologyPaletteManager::reportLoadProblem(const cwError &error)
{
    qWarning("cwSymbologyPaletteManager: %s", qPrintable(error.message()));
    if (!m_errorModel->errors()->contains(error)) {
        m_errorModel->errors()->append(error);
    }
}

void cwSymbologyPaletteManager::reportLoadProblem(const QString &message)
{
    reportLoadProblem(cwError(message, cwError::Warning));
}

cwSymbologyPalette *cwSymbologyPaletteManager::paletteById(const QUuid &id) const
{
    for (auto *palette : m_palettes) {
        if (palette->id() == id) {
            return palette;
        }
    }
    return nullptr;
}

cwSymbologyPalette *cwSymbologyPaletteManager::duplicatePalette(cwSymbologyPalette *source,
                                                               const QString &newName)
{
    if (source == nullptr) {
        reportLoadProblem(QStringLiteral("cannot duplicate a null palette"));
        return nullptr;
    }

    // Copy the value payload (cheap, implicitly-shared) and give the fork a fresh
    // identity. Brush and glyph names are in-palette keys and stay as-is.
    cwSymbologyPaletteData data = source->data();
    const QUuid newId = QUuid::createUuid();
    data.id = newId;
    if (!newName.isEmpty()) {
        data.name = newName;
    }

    if (m_paletteDirectory.isEmpty()) {
        reportLoadProblem(QStringLiteral("cannot duplicate a palette with no project open"));
        return nullptr;
    }

    // Pick a collision-free directory among both on-disk subdirectories and the
    // directories already claimed by live (possibly not-yet-flushed) palettes.
    const QDir root(m_paletteDirectory);
    QSet<QString> takenSubdirs;
    for (auto *existing : std::as_const(m_palettes)) {
        const QString dir = existing->directory();
        if (dir.isEmpty()) { continue; }
        const QFileInfo info(dir);
        if (QDir(info.absolutePath()) == root) {
            takenSubdirs.insert(info.fileName());
        }
    }
    const QString subdirName = uniqueSubdirName(root, slugify(data.name), takenSubdirs);
    const QString targetDir = root.absoluteFilePath(subdirName);

    // Object-first, no synchronous IO: construct the fork in memory and append
    // it. The directory does not exist on disk yet — palettesChanged drives
    // cwSaveLoad to enqueue a first-class full write (palette.json + every glyph
    // and brush), and that job's path creation makes targetDir. The fork is
    // writable and rooted at targetDir; every later glyph/brush edit then
    // persists as its own incremental file job.
    auto *fork = new cwSymbologyPalette(this);
    fork->setData(data);
    fork->setWritable(true);
    fork->setDirectory(targetDir);
    m_palettes.append(fork);

    emit palettesChanged();
    return fork;
}

bool cwSymbologyPaletteManager::saveGlyph(cwSymbologyPalette *palette, const cwSymbologyGlyph &glyph)
{
    if (palette == nullptr) {
        reportLoadProblem(QStringLiteral("cannot save a glyph to a null palette"));
        return false;
    }
    if (!palette->isWritable()) {
        reportLoadProblem(QStringLiteral("palette \"%1\" is read-only; cannot save glyph \"%2\"")
                              .arg(palette->name(), glyph.name));
        return false;
    }

    // Object-first: upsert the glyph on the live palette and emit glyphChanged.
    // cwSaveLoad's connectPalette persists the glyph file asynchronously (a
    // first-class WriteFile job); no synchronous IO and no reload() here, so an
    // in-flight edit can't be clobbered by a stale re-read from disk.
    palette->setGlyph(glyph);
    return true;
}

bool cwSymbologyPaletteManager::removeGlyph(cwSymbologyPalette *palette, const QString &glyphName)
{
    if (palette == nullptr) {
        reportLoadProblem(QStringLiteral("cannot remove a glyph from a null palette"));
        return false;
    }
    if (!palette->isWritable()) {
        reportLoadProblem(QStringLiteral("palette \"%1\" is read-only; cannot remove glyph \"%2\"")
                              .arg(palette->name(), glyphName));
        return false;
    }

    // Object-first: drop the glyph from the live palette (a no-op if absent) and
    // emit glyphChanged. connectPalette removes the glyph file asynchronously;
    // the name's path-safety is enforced there, before it becomes a path.
    palette->removeGlyph(glyphName);
    return true;
}

QString cwSymbologyPaletteManager::duplicateGlyph(cwSymbologyPalette *palette, const QString &name)
{
    if (palette == nullptr) {
        reportLoadProblem(QStringLiteral("cannot duplicate a glyph on a null palette"));
        return QString();
    }
    if (!palette->isWritable()) {
        reportLoadProblem(QStringLiteral("palette \"%1\" is read-only; cannot duplicate glyph \"%2\"")
                              .arg(palette->name(), name));
        return QString();
    }

    const std::optional<cwSymbologyGlyph> source = palette->glyph(name);
    if (!source) {
        reportLoadProblem(QStringLiteral("palette \"%1\" has no glyph \"%2\" to duplicate")
                              .arg(palette->name(), name));
        return QString();
    }

    // Derive a collision-free name against the palette's existing glyph names —
    // not the on-disk directories, so uniqueSubdirName doesn't apply here.
    const QVector<cwSymbologyGlyph> glyphs = palette->glyphs();
    QSet<QString> taken;
    taken.reserve(glyphs.size());
    for (const cwSymbologyGlyph &glyph : glyphs) {
        taken.insert(glyph.name);
    }
    const QString copyName =
        uniqueGlyphName(taken, slugify(name + QStringLiteral("-copy")));

    // Deep value-copy of the strokes; the displayName mirrors the name's "copy"
    // suffix so the two rows read distinctly in the list.
    cwSymbologyGlyph copy = *source;
    copy.name = copyName;
    copy.displayName = source->displayName.isEmpty()
                           ? QString()
                           : source->displayName + QStringLiteral(" copy");

    // Object-first: setGlyph emits glyphChanged; cwSaveLoad writes the file.
    palette->setGlyph(copy);
    return copyName;
}

bool cwSymbologyPaletteManager::removePalette(cwSymbologyPalette *palette)
{
    if (palette == nullptr) {
        reportLoadProblem(QStringLiteral("cannot remove a null palette"));
        return false;
    }
    if (!palette->isWritable()) {
        reportLoadProblem(QStringLiteral("palette \"%1\" is read-only and cannot be removed")
                              .arg(palette->name()));
        return false;
    }
    if (!m_palettes.contains(palette)) {
        reportLoadProblem(QStringLiteral("palette \"%1\" is not managed here").arg(palette->name()));
        return false;
    }

    // Drop it from the list and delete the object (mirrors reload()'s handling
    // of a palette whose directory is gone). palettesChanged then drives
    // cwSaveLoad to tear down the palette's whole directory on the save queue:
    // the teardown reads the stored directory, not this now-deleted object, and
    // the object's destruction auto-clears cwSaveLoad's connection tracking.
    m_palettes.removeOne(palette);
    delete palette;

    emit palettesChanged();
    return true;
}
