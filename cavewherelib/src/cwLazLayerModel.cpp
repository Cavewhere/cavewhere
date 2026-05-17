/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwLazLayerModel.h"

//Qt includes
#include <QDir>
#include <QFileInfo>
#include <QPointer>
#include <QVariant>

//Our includes
#include "cwCavingRegion.h"
#include "cwLazLoader.h"
#include "cwProject.h"

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
    case PointSizeRole:    return layer->pointSize();
    case LoadStatusRole:   return QVariant::fromValue(layer->loadStatus());
    case PointCountRole:   return layer->pointCount();
    default:               return QVariant();
    }
}

QHash<int, QByteArray> cwLazLayerModel::roleNames() const
{
    static const QHash<int, QByteArray> roles = {
        {LayerRole,        "layer"},
        {NameRole,         "name"},
        {SourcePathRole,   "sourcePath"},
        {SourceCSRole,     "sourceCS"},
        {PointSizeRole,    "pointSize"},
        {LoadStatusRole,   "loadStatus"},
        {PointCountRole,   "pointCount"}
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

    // Auto-adopt CS / worldOrigin from the first incoming file before the
    // copy, so the region values are set in time for the post-rescan layers
    // to inherit them. Probing the source path is cheap (header only).
    for (const QUrl& url : urls) {
        if (url.isLocalFile()) {
            maybeAdoptRegionDefaultsFromLaz(url.toLocalFile());
            break;
        }
    }

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
    beginRemoveRows(QModelIndex(), index, index);
    cwLazLayer* layer = m_layers.takeAt(index);
    endRemoveRows();
    layer->deleteLater();
    emit countChanged();
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

void cwLazLayerModel::setRegionGlobalCS(const QString& cs)
{
    if (m_regionGlobalCS == cs) {
        return;
    }
    m_regionGlobalCS = cs;
    for (cwLazLayer* layer : std::as_const(m_layers)) {
        layer->setRegionGlobalCS(cs);
    }
}

void cwLazLayerModel::setRegionWorldOrigin(const cwGeoPoint& origin)
{
    if (m_regionWorldOrigin == origin) {
        return;
    }
    m_regionWorldOrigin = origin;
    for (cwLazLayer* layer : std::as_const(m_layers)) {
        layer->setRegionWorldOrigin(origin);
    }
}

void cwLazLayerModel::setGisLayersDir(const QDir& dir)
{
    if (m_gisLayersDir.absolutePath() == dir.absolutePath()) {
        return;
    }
    m_gisLayersDir = dir;
    // Defer the rescan to the event loop. During project load, this setter
    // is called from cwSaveLoad::setFileName which runs *before* cwCavingRegion
    // ::setData applies the proto's globalCoordinateSystem and worldOrigin.
    // Running rescan synchronously here makes maybeAutoAdoptCS seed values
    // that setData then overwrites with proto defaults (worldOrigin isn't
    // persisted, so it's always {0,0,0} on load). Queuing lets setData run
    // first; auto-adopt then only fills the gaps the proto left unset.
    QMetaObject::invokeMethod(this, &cwLazLayerModel::rescan, Qt::QueuedConnection);
}

void cwLazLayerModel::rescan()
{
    clear();

    if (m_gisLayersDir.absolutePath().isEmpty() || !m_gisLayersDir.exists()) {
        return;
    }

    // Linux's filesystem is case-sensitive, so the upper-case patterns matter;
    // on macOS/Windows the filter is case-insensitive and the duplication is
    // harmless (matches dedupe by absolute path inside entryInfoList).
    const QStringList nameFilters{
        QStringLiteral("*.laz"),
        QStringLiteral("*.las"),
        QStringLiteral("*.LAZ"),
        QStringLiteral("*.LAS")
    };
    const QFileInfoList entries = m_gisLayersDir.entryInfoList(
                nameFilters,
                QDir::Files | QDir::NoDotAndDotDot,
                QDir::Name);

    if (entries.isEmpty()) {
        return;
    }

    // worldOrigin isn't persisted in the proto, so on every load we'd start at
    // (0,0,0) and the geometry would sit at raw UTM coords offscreen. Seeding
    // from the first LAZ's header fills the gap on projects that have LAZ
    // files but no fix stations.
    maybeAdoptRegionDefaultsFromLaz(entries.first().absoluteFilePath());

    QList<cwLazLayer*> newLayers;
    newLayers.reserve(entries.size());
    for (int i = 0; i < entries.size(); ++i) {
        newLayers.append(createLayer());
    }

    beginInsertRows(QModelIndex(), 0, newLayers.size() - 1);
    m_layers = newLayers;
    endInsertRows();

    // Kick off async loads after rows are committed so dataChanged signals
    // from load progress route through valid indices.
    for (int i = 0; i < entries.size(); ++i) {
        m_layers.at(i)->setSourcePath(entries.at(i).absoluteFilePath());
    }

    emit countChanged();
}

void cwLazLayerModel::clear()
{
    if (m_layers.isEmpty()) {
        return;
    }
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
    connect(layer, &cwLazLayer::sourceCSChanged, this, [emitForRole]() { emitForRole(SourceCSRole); });
    connect(layer, &cwLazLayer::pointSizeChanged, this, [emitForRole]() { emitForRole(PointSizeRole); });
    connect(layer, &cwLazLayer::loadStatusChanged, this, [emitForRole]() { emitForRole(LoadStatusRole); });
    connect(layer, &cwLazLayer::pointCountChanged, this, [emitForRole]() { emitForRole(PointCountRole); });
}

cwLazLayer* cwLazLayerModel::createLayer()
{
    auto* layer = new cwLazLayer(this);
    connectLayer(layer);
    layer->setFutureManagerToken(m_futureManagerToken);
    layer->setRegionGlobalCS(m_regionGlobalCS);
    layer->setRegionWorldOrigin(m_regionWorldOrigin);
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

void cwLazLayerModel::maybeAdoptRegionDefaultsFromLaz(const QString& sourcePath)
{
    auto* region = qobject_cast<cwCavingRegion*>(parent());
    if (region == nullptr) {
        return;
    }

    const bool needsCS = region->globalCoordinateSystem().isEmpty();
    const bool needsOrigin = (region->worldOrigin() == cwGeoPoint{});

    if (!needsCS && !needsOrigin) {
        return;
    }

    const auto probe = cwLazLoader::probeHeader(sourcePath);
    if (!probe.valid) {
        return;
    }

    // setGlobalCoordinateSystem resets worldOrigin to {} internally; the
    // setWorldOrigin call below restores a meaningful origin from the bbox
    // center. Order matters — flip these and the origin gets wiped after we
    // set it.
    if (needsCS && !probe.sourceCS.isEmpty()) {
        region->setGlobalCoordinateSystem(probe.sourceCS);
    }
    if (needsOrigin) {
        region->setWorldOrigin(probe.bboxCenter);
    }
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
