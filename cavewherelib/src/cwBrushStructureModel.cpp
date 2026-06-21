/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwBrushStructureModel.h"
#include "cwBrushEditor.h"
// cwBrushEditor exposes inline QPointer<...> getters over types it only
// forward-declares; this TU instantiates them, so pull in the complete types.
#include "cwSymbologyPalette.h"
#include "cwSketchCanvas.h"
#include "cwCavingRegion.h"
#include "cwScrapManager.h"
#include "cwTrip.h"

cwBrushStructureModel::cwBrushStructureModel(cwBrushEditor *editor) :
    QAbstractItemModel(editor),
    m_editor(editor)
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
    if (m_editor == nullptr) {
        return 0;
    }
    if (!parent.isValid()) {
        return m_editor->layerCount();
    }
    if (parent.internalId() == kLayerId) {
        return m_editor->ruleCount(parent.row());
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
    if (m_editor == nullptr || !index.isValid()) {
        return {};
    }

    const bool isLayer = index.internalId() == kLayerId;
    const int layerIndex = isLayer ? index.row() : static_cast<int>(index.internalId());
    const int ruleIndex = isLayer ? -1 : index.row();

    switch (role) {
    case Qt::DisplayRole:
    case LabelRole:
        return isLayer ? m_editor->layerLabel(layerIndex)
                       : m_editor->ruleName(layerIndex, ruleIndex);
    case RuleEnabledRole:
        return isLayer ? true : m_editor->ruleEnabled(layerIndex, ruleIndex);
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

void cwBrushStructureModel::reset()
{
    beginResetModel();
    endResetModel();
}

void cwBrushStructureModel::ruleChanged(int layerIndex, int ruleIndex)
{
    if (m_editor == nullptr) {
        return;
    }
    const QModelIndex layer = index(layerIndex, 0, QModelIndex());
    const QModelIndex rule = index(ruleIndex, 0, layer);
    if (rule.isValid()) {
        emit dataChanged(rule, rule, {RuleEnabledRole, Qt::DisplayRole, LabelRole});
    }
}
