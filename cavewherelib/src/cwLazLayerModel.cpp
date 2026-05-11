/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwLazLayerModel.h"

//Qt includes
#include <QVariant>

//Our includes
#include "cavewhere.pb.h"
#include "cwCavingRegion.h"
#include "cwLazLoader.h"

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
    return {
        {LayerRole,        "layer"},
        {NameRole,         "name"},
        {SourcePathRole,   "sourcePath"},
        {SourceCSRole,     "sourceCS"},
        {PointSizeRole,    "pointSize"},
        {LoadStatusRole,   "loadStatus"},
        {PointCountRole,   "pointCount"}
    };
}

cwLazLayer* cwLazLayerModel::addLayer(const QString& sourcePath)
{
    maybeAutoAdoptCS(sourcePath);

    auto* layer = new cwLazLayer(this);
    connectLayer(layer);

    layer->setFutureManagerToken(m_futureManagerToken);
    layer->setRegionGlobalCS(m_regionGlobalCS);
    layer->setRegionWorldOrigin(m_regionWorldOrigin);

    const int row = m_layers.size();
    beginInsertRows(QModelIndex(), row, row);
    m_layers.append(layer);
    endInsertRows();

    layer->setSourcePath(sourcePath);

    emit countChanged();
    return layer;
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
    for (cwLazLayer* layer : m_layers) {
        layer->setFutureManagerToken(token);
    }
}

void cwLazLayerModel::setRegionGlobalCS(const QString& cs)
{
    if (m_regionGlobalCS == cs) {
        return;
    }
    m_regionGlobalCS = cs;
    for (cwLazLayer* layer : m_layers) {
        layer->setRegionGlobalCS(cs);
    }
}

void cwLazLayerModel::setRegionWorldOrigin(const cwGeoPoint& origin)
{
    if (m_regionWorldOrigin == origin) {
        return;
    }
    m_regionWorldOrigin = origin;
    for (cwLazLayer* layer : m_layers) {
        layer->setRegionWorldOrigin(origin);
    }
}

void cwLazLayerModel::writeTo(CavewhereProto::ProjectMetadata* metadata) const
{
    metadata->clear_lazlayers();
    for (cwLazLayer* layer : m_layers) {
        CavewhereProto::LazLayer* protoLayer = metadata->add_lazlayers();
        protoLayer->set_sourcepath(layer->sourcePath().toStdString());
    }
}

void cwLazLayerModel::readFrom(const CavewhereProto::ProjectMetadata& metadata)
{
    clear();
    if (metadata.lazlayers_size() == 0) {
        return;
    }

    QList<cwLazLayer*> newLayers;
    newLayers.reserve(metadata.lazlayers_size());
    for (int i = 0; i < metadata.lazlayers_size(); ++i) {
        auto* layer = new cwLazLayer(this);
        connectLayer(layer);
        layer->setFutureManagerToken(m_futureManagerToken);
        layer->setRegionGlobalCS(m_regionGlobalCS);
        layer->setRegionWorldOrigin(m_regionWorldOrigin);
        newLayers.append(layer);
    }

    beginInsertRows(QModelIndex(), 0, metadata.lazlayers_size() - 1);
    m_layers = newLayers;
    endInsertRows();

    // Kick off async loads after the rows are committed so dataChanged
    // signals from in-flight load progress route through valid indices.
    for (int i = 0; i < metadata.lazlayers_size(); ++i) {
        m_layers[i]->setSourcePath(
            QString::fromStdString(metadata.lazlayers(i).sourcepath()));
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

int cwLazLayerModel::indexOf(cwLazLayer* layer) const
{
    for (int i = 0; i < m_layers.size(); ++i) {
        if (m_layers.at(i) == layer) {
            return i;
        }
    }
    return -1;
}

void cwLazLayerModel::maybeAutoAdoptCS(const QString& sourcePath)
{
    auto* region = qobject_cast<cwCavingRegion*>(parent());
    if (region == nullptr) {
        return;
    }
    if (!region->globalCS().isEmpty()) {
        return;
    }

    const auto probe = cwLazLoader::probeHeader(sourcePath);
    if (!probe.valid) {
        return;
    }

    // setGlobalCS resets worldOrigin to {} internally; the setWorldOrigin
    // call below restores a meaningful origin from the bbox center. Order
    // matters — flip these and the origin gets wiped after we set it.
    if (!probe.sourceCS.isEmpty()) {
        region->setGlobalCS(probe.sourceCS);
    }
    region->setWorldOrigin(probe.bboxCenter);
}
