/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwSymbologyPaletteManager.h"
#include "cwSymbologyPalette.h"
#include "cwSymbologyPaletteSeed.h"
#include "cwSymbologyPaletteIO.h"
#include "cwError.h"
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
    m_paletteDirectory(defaultPaletteDirectory())
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
    m_seed = nullptr;
}

void cwSymbologyPaletteManager::reload()
{
    clearInstalled();
    m_loadErrors.clear();

    // The seed is always present and always wins its id.
    m_seed = new cwSymbologyPalette(this);
    m_seed->setData(cwSymbologyPaletteSeed::create());
    m_seed->setWritable(false);
    m_palettes.append(m_seed);

    QHash<QUuid, QString> directoryById;
    directoryById.insert(m_seed->id(), QStringLiteral("<built-in seed>"));

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
        }
    }

    emit palettesChanged();
}

void cwSymbologyPaletteManager::setErrorModel(cwErrorListModel *model)
{
    if (m_errorModel == model) { return; }
    m_errorModel = model;

    // The constructor's reload() ran before the app could wire a model; replay
    // its accumulated problems so they reach the UI now.
    for (const QString &message : std::as_const(m_loadErrors)) {
        postToErrorModel(message);
    }
}

void cwSymbologyPaletteManager::reportLoadProblem(const QString &message)
{
    m_loadErrors.append(message);
    qWarning("cwSymbologyPaletteManager: %s", qPrintable(message));
    postToErrorModel(message);
}

void cwSymbologyPaletteManager::postToErrorModel(const QString &message)
{
    if (m_errorModel == nullptr) { return; }
    const cwError error(message, cwError::Warning);
    if (!m_errorModel->contains(error)) {
        m_errorModel->append(error);
    }
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
