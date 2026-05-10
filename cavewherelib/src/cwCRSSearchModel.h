/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWCRSSEARCHMODEL_H
#define CWCRSSEARCHMODEL_H

//Qt includes
#include <QAbstractListModel>
#include <QString>
#include <QtQml/qqmlregistration.h>

//Our includes
#include "cwGlobals.h"

/**
 * QAbstractListModel exposing PROJ's CRS database to QML so the user can
 * search for a coordinate system by name or EPSG code.
 *
 * On first access, queries PROJ for every EPSG CRS (proj.db is shipped
 * alongside libproj and has ~7000 entries). With an empty query, the model
 * exposes the curated cwCoordinateTransform::commonProjectedCSList(); with
 * a non-empty query, filters the full list by name (case-insensitive
 * substring) or by EPSG code (numeric prefix match).
 */
class CAVEWHERE_LIB_EXPORT cwCRSSearchModel : public QAbstractListModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(CRSSearchModel)

    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)

public:
    enum Roles {
        AuthNameRole = Qt::UserRole + 1,
        CodeRole,
        DisplayCodeRole,
        NameRole,
        IsProjectedRole
    };
    Q_ENUM(Roles)

    explicit cwCRSSearchModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    Q_INVOKABLE QModelIndex index(int row, int column = 0, const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString query() const { return m_query; }
    void setQuery(const QString& q);

signals:
    void queryChanged();

private:
    struct Row {
        QString authName;       // "EPSG"
        QString code;           // "32616"
        QString name;           // "WGS 84 / UTM zone 16N"
        bool    projected = false;
    };

    void ensurePopulated() const;
    void applyFilter();
    const QList<int>& emptyQueryIndices() const;

    mutable bool m_populated = false;
    mutable QList<Row> m_all;
    mutable QList<int> m_emptyQueryIndices;
    mutable bool m_emptyQueryIndicesBuilt = false;
    QList<int> m_filteredIndices;
    QString m_query;
};

#endif // CWCRSSEARCHMODEL_H
