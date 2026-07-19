/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSCOPESTATIONLISTMODEL_H
#define CWSCOPESTATIONLISTMODEL_H

//Qt includes
#include <QAbstractListModel>
#include <QString>
#include <QVector3D>
#include <QtQml/qqmlregistration.h>

//Our includes
#include "cwGlobals.h"
#include "cwSurveyNetwork.h"

/**
 * Read-only list model exposing the post-solve stations whose qualified
 * name starts with scopePrefix (typically "cave_<uuid>.trip_<uuid>.").
 *
 * The network is a plain value property; bind it to
 * cwLinePlotManager::regionNetwork so the model repopulates (full
 * modelReset, no incremental diff) whenever the line-plot pipeline
 * publishes a new network. Rows are sorted lexicographically by
 * qualified name so consumers like the carpet-picker autocomplete are
 * deterministic. An empty scopePrefix yields zero rows rather than
 * every station in the region.
 */
class CAVEWHERE_LIB_EXPORT cwScopeStationListModel : public QAbstractListModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(ScopeStationListModel)

    Q_PROPERTY(QString scopePrefix READ scopePrefix WRITE setScopePrefix NOTIFY scopePrefixChanged)
    Q_PROPERTY(cwSurveyNetwork network READ network WRITE setNetwork NOTIFY networkChanged)

public:
    enum Roles {
        StationNameRole = Qt::UserRole + 1, //prefix-stripped display name
        QualifiedNameRole,                  //full qualified name
        PositionRole                        //QVector3D from the network
    };
    Q_ENUM(Roles)

    explicit cwScopeStationListModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString scopePrefix() const { return m_scopePrefix; }
    void setScopePrefix(const QString& scopePrefix);

    cwSurveyNetwork network() const { return m_network; }
    void setNetwork(const cwSurveyNetwork& network);

signals:
    void scopePrefixChanged();
    void networkChanged();

private:
    struct Row {
        QString displayName;
        QString qualifiedName;
        QVector3D position;
    };

    void rebuildRows();

    QString m_scopePrefix;
    cwSurveyNetwork m_network;
    QList<Row> m_rows;
};

#endif // CWSCOPESTATIONLISTMODEL_H
