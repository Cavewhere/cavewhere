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
#include <QStandardPaths>
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

// First "<slug>", "<slug>-2", "<slug>-3"… not already a subdirectory of root.
QString uniqueSubdirName(const QDir &root, const QString &slug)
{
    QString candidate = slug;
    int suffix = 2;
    while (root.exists(candidate)) {
        candidate = QStringLiteral("%1-%2").arg(slug).arg(suffix++);
    }
    return candidate;
}

}

cwSymbologyPaletteManager *cwSymbologyPaletteManager::Singleton = nullptr;

cwSymbologyPaletteManager::cwSymbologyPaletteManager(QObject *parent) :
    QObject(parent),
    m_paletteDirectory(defaultPaletteDirectory()),
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

QString cwSymbologyPaletteManager::defaultPaletteDirectory()
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return QDir(base).filePath(QStringLiteral("palettes"));
}

void cwSymbologyPaletteManager::setPaletteDirectory(const QString &directory)
{
    if (m_paletteDirectory == directory) { return; }
    m_paletteDirectory = directory;
    reload();
}

void cwSymbologyPaletteManager::reload()
{
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

    const QDir root(m_paletteDirectory);
    if (root.exists()) {
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

    QDir root(m_paletteDirectory);
    if (!root.exists() && !root.mkpath(QStringLiteral("."))) {
        reportLoadProblem(
            QStringLiteral("failed to create palette directory \"%1\"").arg(m_paletteDirectory));
        return nullptr;
    }

    const QString targetDir = root.filePath(uniqueSubdirName(root, slugify(data.name)));
    const auto result = cwSymbologyPaletteIO::save(data, targetDir);
    if (result.hasError()) {
        reportLoadProblem(
            QStringLiteral("failed to write palette \"%1\": %2").arg(targetDir, result.errorMessage()));
        return nullptr;
    }

    reload();
    return paletteById(newId);
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

    const auto result = cwSymbologyPaletteIO::saveGlyph(glyph, palette->directory());
    if (result.hasError()) {
        reportLoadProblem(
            QStringLiteral("failed to save glyph \"%1\": %2").arg(glyph.name, result.errorMessage()));
        return false;
    }

    reload(); // reconciles by id — `palette` stays valid and picks up the glyph
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

    const auto result = cwSymbologyPaletteIO::removeGlyph(palette->directory(), glyphName);
    if (result.hasError()) {
        reportLoadProblem(result.errorMessage());
        return false;
    }

    reload(); // reconciles by id — `palette` stays valid and drops the glyph
    return true;
}
