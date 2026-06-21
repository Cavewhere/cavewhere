/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwBrushStructureModel.h"

cwBrushStructureModel::cwBrushStructureModel(QObject *parent) :
    QAbstractItemModel(parent)
{
}

QModelIndex cwBrushStructureModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent)) {
        return {};
    }
    if (!parent.isValid()) {
        return createIndex(row, column, kLayerId);                       // a layer row
    }
    if (parent.internalId() == kLayerId) {
        return createIndex(row, column, static_cast<quintptr>(parent.row())); // a rule row
    }
    return {};
}

QModelIndex cwBrushStructureModel::parent(const QModelIndex &index) const
{
    if (!index.isValid() || index.internalId() == kLayerId) {
        return {};
    }
    return createIndex(static_cast<int>(index.internalId()), 0, kLayerId);
}

int cwBrushStructureModel::rowCount(const QModelIndex &parent) const
{
    if (!parent.isValid()) {
        return layerCount();
    }
    if (parent.internalId() == kLayerId) {
        return ruleCount(parent.row());
    }
    return 0;   // rule rows are leaves
}

int cwBrushStructureModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return 1;
}

QVariant cwBrushStructureModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return {};
    }

    const bool isLayer = index.internalId() == kLayerId;
    const int layerIndex = isLayer ? index.row() : static_cast<int>(index.internalId());
    const int ruleIndex = isLayer ? -1 : index.row();

    switch (role) {
    case Qt::DisplayRole:
    case LabelRole:
        return isLayer ? layerLabel(layerIndex) : ruleName(layerIndex, ruleIndex);
    case RuleEnabledRole:
        return isLayer ? true : ruleEnabled(layerIndex, ruleIndex);
    case IsLayerRole:
        return isLayer;
    case LayerIndexRole:
        return layerIndex;
    case RuleIndexRole:
        return ruleIndex;
    default:
        return {};
    }
}

QHash<int, QByteArray> cwBrushStructureModel::roleNames() const
{
    return {
        {Qt::DisplayRole, "display"},
        {LabelRole, "label"},
        {RuleEnabledRole, "ruleEnabled"},
        {IsLayerRole, "isLayer"},
        {LayerIndexRole, "layerIndex"},
        {RuleIndexRole, "ruleIndex"},
    };
}

void cwBrushStructureModel::setBrush(const cwLineBrush &brush)
{
    beginResetModel();
    m_brush = brush;
    endResetModel();
}

int cwBrushStructureModel::layerCount() const
{
    return static_cast<int>(m_brush.decorations.size());
}

QString cwBrushStructureModel::layerLabel(int layerIndex) const
{
    if (!layerInRange(layerIndex)) {
        return QString();
    }
    return m_brush.decorations.at(layerIndex).glyphName;   // empty for a line layer
}

int cwBrushStructureModel::ruleCount(int layerIndex) const
{
    if (!layerInRange(layerIndex)) {
        return 0;
    }
    return static_cast<int>(m_brush.decorations.at(layerIndex).rules.size());
}

QString cwBrushStructureModel::ruleName(int layerIndex, int ruleIndex) const
{
    if (!layerInRange(layerIndex)) {
        return QString();
    }
    const cwDecorationLayer &layer = m_brush.decorations.at(layerIndex);
    if (ruleIndex < 0 || ruleIndex >= layer.rules.size()) {
        return QString();
    }
    return layer.rules.at(ruleIndex).name;
}

bool cwBrushStructureModel::ruleEnabled(int layerIndex, int ruleIndex) const
{
    if (!layerInRange(layerIndex)) {
        return false;
    }
    const cwDecorationLayer &layer = m_brush.decorations.at(layerIndex);
    if (ruleIndex < 0 || ruleIndex >= layer.rules.size()) {
        return false;
    }
    return layer.rules.at(ruleIndex).enabled;
}

bool cwBrushStructureModel::setRuleEnabled(int layerIndex, int ruleIndex, bool enabled)
{
    if (!layerInRange(layerIndex)) {
        return false;
    }
    cwDecorationLayer layer = m_brush.decorations.at(layerIndex);
    if (ruleIndex < 0 || ruleIndex >= layer.rules.size()) {
        return false;
    }
    if (layer.rules.at(ruleIndex).enabled == enabled) {
        return false;
    }

    cwPlacementRuleData rule = layer.rules.at(ruleIndex);
    rule.enabled = enabled;
    layer.rules.replace(ruleIndex, rule);
    m_brush.decorations.replace(layerIndex, layer);

    const QModelIndex ruleRow = index(ruleIndex, 0, index(layerIndex, 0, QModelIndex()));
    emit dataChanged(ruleRow, ruleRow, {RuleEnabledRole, Qt::DisplayRole, LabelRole});
    return true;
}

bool cwBrushStructureModel::insertRule(int layerIndex, int ruleIndex, const cwPlacementRuleData &rule)
{
    if (!layerInRange(layerIndex)) {
        return false;
    }
    cwDecorationLayer layer = m_brush.decorations.at(layerIndex);
    if (ruleIndex < 0 || ruleIndex > layer.rules.size()) {   // == size appends
        return false;
    }

    beginInsertRows(index(layerIndex, 0, QModelIndex()), ruleIndex, ruleIndex);
    layer.rules.insert(ruleIndex, rule);
    m_brush.decorations.replace(layerIndex, layer);
    endInsertRows();
    return true;
}

bool cwBrushStructureModel::removeRule(int layerIndex, int ruleIndex)
{
    if (!layerInRange(layerIndex)) {
        return false;
    }
    cwDecorationLayer layer = m_brush.decorations.at(layerIndex);
    if (ruleIndex < 0 || ruleIndex >= layer.rules.size()) {
        return false;
    }

    beginRemoveRows(index(layerIndex, 0, QModelIndex()), ruleIndex, ruleIndex);
    layer.rules.removeAt(ruleIndex);
    m_brush.decorations.replace(layerIndex, layer);
    endRemoveRows();
    return true;
}

bool cwBrushStructureModel::layerInRange(int layerIndex) const
{
    return layerIndex >= 0 && layerIndex < m_brush.decorations.size();
}
