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
                qWarning("cwSymbologyPaletteManager: %s",
                         qPrintable(result.errorMessage()));
                m_loadErrors.append(result.errorMessage());
                continue;
            }
            const cwSymbologyPaletteData data = result.value();

            const auto existing = directoryById.constFind(data.id);
            if (existing != directoryById.constEnd()) {
                const QString message =
                    QStringLiteral("duplicate palette id %1 in \"%2\"; keeping the one from \"%3\"")
                        .arg(data.id.toString(QUuid::WithoutBraces),
                             entry.absoluteFilePath(),
                             existing.value());
                qWarning("cwSymbologyPaletteManager: %s", qPrintable(message));
                m_loadErrors.append(message);
                continue;
            }

            auto *palette = new cwSymbologyPalette(this);
            palette->setData(data);
            palette->setWritable(true);
            directoryById.insert(data.id, entry.absoluteFilePath());
            m_palettes.append(palette);
        }
    }

    emit palettesChanged();
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
