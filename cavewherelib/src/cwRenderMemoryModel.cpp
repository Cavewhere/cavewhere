/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwRenderMemoryModel.h"
#include "cwRenderMemoryLedger.h"

//Std includes
#include <iterator>

namespace {

constexpr int kPollIntervalMilliseconds = 500;
constexpr qint64 kBytesPerKilobyte = 1024;
constexpr int kDecimalPlaces = 1;

using Category = cwRenderMemoryLedger::Category;
using Residency = cwRenderMemoryLedger::Residency;

struct CategoryDescription {
    Category category;
    const char* name;
};

constexpr CategoryDescription kCategories[] = {
    {Category::PointCloudGeometry, QT_TRANSLATE_NOOP("cwRenderMemoryModel", "Point cloud geometry")},
    {Category::TexturedItemGeometry, QT_TRANSLATE_NOOP("cwRenderMemoryModel", "Textured item geometry")},
    {Category::TexturedItemTexture, QT_TRANSLATE_NOOP("cwRenderMemoryModel", "Textured item textures")},
    {Category::LinePlotGeometry, QT_TRANSLATE_NOOP("cwRenderMemoryModel", "Line plot geometry")},
    {Category::Other, QT_TRANSLATE_NOOP("cwRenderMemoryModel", "Other")}
};

} // namespace

cwRenderMemoryModel::cwRenderMemoryModel(QObject* parent) :
    QAbstractListModel(parent),
    m_rows(static_cast<int>(std::size(kCategories)))
{
    m_timer.setInterval(kPollIntervalMilliseconds);
    connect(&m_timer, &QTimer::timeout, this, &cwRenderMemoryModel::poll);

    refresh();
}

int cwRenderMemoryModel::rowCount(const QModelIndex& parent) const
{
    if(parent.isValid()) {
        return 0;
    }
    return m_rows.size();
}

QVariant cwRenderMemoryModel::data(const QModelIndex& index, int role) const
{
    if(index.row() < 0 || index.row() >= m_rows.size()) {
        return QVariant();
    }

    const Row& row = m_rows.at(index.row());

    switch(role) {
    case NameRole:
        return tr(kCategories[index.row()].name);
    case GpuBytesRole:
        return row.gpuBytes;
    case CpuBytesRole:
        return row.cpuBytes;
    case GpuTextRole:
        return formattedBytes(row.gpuBytes);
    case CpuTextRole:
        return formattedBytes(row.cpuBytes);
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> cwRenderMemoryModel::roleNames() const
{
    return {
        {NameRole, "name"},
        {GpuBytesRole, "gpuBytes"},
        {CpuBytesRole, "cpuBytes"},
        {GpuTextRole, "gpuText"},
        {CpuTextRole, "cpuText"}
    };
}

void cwRenderMemoryModel::setRunning(bool running)
{
    if(m_running == running) {
        return;
    }

    m_running = running;

    if(m_running) {
        m_timer.start();
        refresh();
    } else {
        m_timer.stop();
    }

    emit runningChanged();
}

void cwRenderMemoryModel::poll()
{
    if(cwRenderMemoryLedger::instance()->revision() == m_lastRevision) {
        return;
    }
    refresh();
}

void cwRenderMemoryModel::refresh()
{
    auto* ledger = cwRenderMemoryLedger::instance();
    m_lastRevision = ledger->revision();

    for(int i = 0; i < m_rows.size(); i++) {
        const Category category = kCategories[i].category;
        Row& row = m_rows[i];
        const qint64 gpuBytes = ledger->bytes(category, Residency::Gpu);
        const qint64 cpuBytes = ledger->bytes(category, Residency::Cpu);

        if(row.gpuBytes == gpuBytes && row.cpuBytes == cpuBytes) {
            continue;
        }

        row.gpuBytes = gpuBytes;
        row.cpuBytes = cpuBytes;

        const QModelIndex changedIndex = index(i);
        emit dataChanged(changedIndex, changedIndex,
                         {GpuBytesRole, CpuBytesRole, GpuTextRole, CpuTextRole});
    }

    const qint64 totalGpuBytes = ledger->totalBytes(Residency::Gpu);
    const qint64 totalCpuBytes = ledger->totalBytes(Residency::Cpu);

    if(m_totalGpuBytes != totalGpuBytes || m_totalCpuBytes != totalCpuBytes) {
        m_totalGpuBytes = totalGpuBytes;
        m_totalCpuBytes = totalCpuBytes;
        emit totalsChanged();
    }
}

QString cwRenderMemoryModel::formattedBytes(qint64 bytes)
{
    if(bytes < kBytesPerKilobyte) {
        return tr("%1 B").arg(bytes);
    }

    constexpr const char* suffixes[] = {
        QT_TRANSLATE_NOOP("cwRenderMemoryModel", "%1 KB"),
        QT_TRANSLATE_NOOP("cwRenderMemoryModel", "%1 MB"),
        QT_TRANSLATE_NOOP("cwRenderMemoryModel", "%1 GB")
    };

    double scaled = static_cast<double>(bytes) / static_cast<double>(kBytesPerKilobyte);
    size_t suffixIndex = 0;

    while(scaled >= kBytesPerKilobyte && suffixIndex + 1 < std::size(suffixes)) {
        scaled /= static_cast<double>(kBytesPerKilobyte);
        suffixIndex++;
    }

    return tr(suffixes[suffixIndex]).arg(QString::number(scaled, 'f', kDecimalPlaces));
}
