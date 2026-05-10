/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwCRSSearchModel.h"
#include "cwCoordinateTransform.h"
#include "cwCoordinateTransformPrivate.h"

//Qt includes
#include <QSet>

namespace {
    PJ_CONTEXT* searchContext()
    {
        // Reuse a per-thread context with the cwCoordinateTransform-configured
        // search paths so proj.db is found wherever it's been deployed.
        thread_local struct Holder {
            PJ_CONTEXT* ctx = cwCoordinateTransformPrivate::createContext();
            ~Holder() { if (ctx) { proj_context_destroy(ctx); } }
        } holder;
        return holder.ctx;
    }
}

cwCRSSearchModel::cwCRSSearchModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

QModelIndex cwCRSSearchModel::index(int row, int column, const QModelIndex& parent) const
{
    return QAbstractListModel::index(row, column, parent);
}

const QList<int>& cwCRSSearchModel::emptyQueryIndices() const
{
    if (m_emptyQueryIndicesBuilt) {
        return m_emptyQueryIndices;
    }
    m_emptyQueryIndicesBuilt = true;
    m_emptyQueryIndices.clear();

    const QStringList common = cwCoordinateTransform::commonProjectedCSList();
    QSet<QString> commonCodes;
    commonCodes.reserve(common.size());
    for (const QString& cs : common) {
        const int colon = cs.indexOf(':');
        if (colon > 0) {
            commonCodes.insert(cs.mid(colon + 1));
        }
    }
    for (int i = 0; i < m_all.size(); ++i) {
        if (commonCodes.contains(m_all.at(i).code)) {
            m_emptyQueryIndices.append(i);
        }
    }
    return m_emptyQueryIndices;
}

int cwCRSSearchModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    ensurePopulated();
    if (m_query.trimmed().isEmpty()) {
        return emptyQueryIndices().size();
    }
    return m_filteredIndices.size();
}

QVariant cwCRSSearchModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid()) {
        return {};
    }
    ensurePopulated();

    const QList<int>& indices = m_query.trimmed().isEmpty()
        ? emptyQueryIndices()
        : m_filteredIndices;
    if (index.row() < 0 || index.row() >= indices.size()) {
        return {};
    }
    const Row* row = &m_all.at(indices.at(index.row()));

    switch (role) {
    case AuthNameRole:    return row->authName;
    case CodeRole:        return row->code;
    case DisplayCodeRole: return QStringLiteral("%1:%2").arg(row->authName, row->code);
    case NameRole:        return row->name;
    case IsProjectedRole: return row->projected;
    }
    return {};
}

QHash<int, QByteArray> cwCRSSearchModel::roleNames() const
{
    return {
        {AuthNameRole,    "authName"},
        {CodeRole,        "code"},
        {DisplayCodeRole, "displayCode"},
        {NameRole,        "name"},
        {IsProjectedRole, "isProjected"}
    };
}

void cwCRSSearchModel::setQuery(const QString& q)
{
    if (q == m_query) {
        return;
    }
    m_query = q;
    emit queryChanged();
    beginResetModel();
    applyFilter();
    endResetModel();
}

void cwCRSSearchModel::ensurePopulated() const
{
    if (m_populated) {
        return;
    }
    m_populated = true;

    PJ_CONTEXT* ctx = searchContext();
    if (!ctx) {
        return;
    }

    int count = 0;
    PROJ_CRS_LIST_PARAMETERS* params = proj_get_crs_list_parameters_create();
    if (!params) {
        return;
    }
    const PJ_TYPE wantedTypes[] = {
        PJ_TYPE_PROJECTED_CRS,
        PJ_TYPE_GEOGRAPHIC_2D_CRS,
        PJ_TYPE_GEOGRAPHIC_3D_CRS
    };
    params->types = wantedTypes;
    params->typesCount = sizeof(wantedTypes) / sizeof(wantedTypes[0]);
    params->crs_area_of_use_contains_bbox = 0;
    params->bbox_valid = 0;
    params->allow_deprecated = 0;

    PROJ_CRS_INFO** infos = proj_get_crs_info_list_from_database(
        ctx, "EPSG", params, &count);
    proj_get_crs_list_parameters_destroy(params);

    if (!infos) {
        return;
    }

    m_all.reserve(count);
    for (int i = 0; i < count; ++i) {
        const PROJ_CRS_INFO* info = infos[i];
        if (!info || !info->code) {
            continue;
        }
        Row row;
        row.authName = QString::fromUtf8(info->auth_name ? info->auth_name : "EPSG");
        row.code     = QString::fromUtf8(info->code);
        row.name     = QString::fromUtf8(info->name ? info->name : "");
        row.projected = info->type == PJ_TYPE_PROJECTED_CRS;
        m_all.append(row);
    }
    proj_crs_info_list_destroy(infos);
}

void cwCRSSearchModel::applyFilter()
{
    m_filteredIndices.clear();
    ensurePopulated();
    const QString q = m_query.trimmed();
    if (q.isEmpty()) {
        return; // Empty-query path is handled directly in rowCount/data.
    }

    // Numeric query (or "EPSG:nnn" prefix): match by code prefix.
    QString numericPart = q;
    if (numericPart.startsWith(QStringLiteral("EPSG:"), Qt::CaseInsensitive)) {
        numericPart = numericPart.mid(5).trimmed();
    }
    bool numeric = !numericPart.isEmpty();
    for (QChar c : numericPart) {
        if (!c.isDigit()) {
            numeric = false;
            break;
        }
    }

    for (int i = 0; i < m_all.size(); ++i) {
        const Row& r = m_all.at(i);
        bool match = false;
        if (numeric) {
            match = r.code.startsWith(numericPart);
        } else {
            match = r.name.contains(q, Qt::CaseInsensitive);
        }
        if (match) {
            m_filteredIndices.append(i);
        }
    }
}
