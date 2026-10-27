/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWEXTERNALSOURCEMODEL_H
#define CWEXTERNALSOURCEMODEL_H

//Qt includes
#include <QAbstractListModel>
#include <QList>
#include <QQmlEngine>

//Our includes
#include "cwExternalSource.h"
#include "cwGlobals.h"

/**
 * QAbstractListModel of cwExternalSource rows, one per attached survey source.
 * The cwCavingRegion owns the one list for the project.
 *
 * The persistence layer saves the project file off this model's row signals,
 * so a source added, removed, or replaced here reaches disk.
 */
class CAVEWHERE_LIB_EXPORT cwExternalSourceModel : public QAbstractListModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(ExternalSourceModel)
    QML_UNCREATABLE("Owned by the caving region")

    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Roles {
        SourceRole = Qt::UserRole + 1,
        IdRole,
        RootNodeIdRole
    };
    Q_ENUM(Roles)

    explicit cwExternalSourceModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void appendSource(const cwExternalSource& source);
    void removeAt(int index);
    //! Replaces the source at \a index in place, one dataChanged() for the row.
    void replaceAt(int index, const cwExternalSource& source);
    cwExternalSource sourceAt(int index) const;
    //! The row of the source whose id is \a id, or -1.
    int indexOf(const QUuid& id) const;

    void setSources(const QList<cwExternalSource>& sources);
    const QList<cwExternalSource>& sources() const { return m_sources; }
    int count() const { return m_sources.size(); }

signals:
    void countChanged();

private:
    QList<cwExternalSource> m_sources;
};

#endif // CWEXTERNALSOURCEMODEL_H
