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

void cwSymbologyPaletteManager::clearInstalled()
{
    for (auto *palette : std::as_const(m_palettes)) {
        delete palette;
    }
    m_palettes.clear();
    m_default = nullptr;
}

void cwSymbologyPaletteManager::reload()
{
    clearInstalled();
    m_errorModel->errors()->clear();

    QHash<QUuid, QString> directoryById;

    // The default palette is shipped embedded as a qrc resource and loaded
    // through the normal IO path. It is always present, read-only, and wins its
    // id on duplicates. A load failure here is a build defect (corrupt embedded
    // resource): report it and leave m_default null — there is no code-built
    // fallback.
    const QString defaultPath = QStringLiteral(":/palettes/cavewhere-default");
    const auto defaultResult = cwSymbologyPaletteIO::load(defaultPath);
    if (defaultResult.hasError()) {
        reportLoadProblem(
            QStringLiteral("failed to load the built-in default palette \"%1\": %2")
                .arg(defaultPath, defaultResult.errorMessage()));
    } else {
        const auto &loadResult = defaultResult.value();
        m_default = new cwSymbologyPalette(this);
        m_default->setData(loadResult.palette);
        m_default->setWritable(false);
        m_palettes.append(m_default);
        directoryById.insert(m_default->id(), defaultPath);

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

            auto *palette = new cwSymbologyPalette(this);
            palette->setData(data);
            palette->setWritable(versionSupported);
            directoryById.insert(data.id, entry.absoluteFilePath());
            m_palettes.append(palette);

            // Rule-stack warnings for an accepted palette (commit 4.4). Fatal
            // problems already failed the load above, so these are all warnings.
            for (const cwError &warning : std::as_const(loadResult.warnings)) {
                reportLoadProblem(warning);
            }
        }
    }

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
