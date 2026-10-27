/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWEQUATEMODEL_H
#define CWEQUATEMODEL_H

//Qt includes
#include <QAbstractListModel>
#include <QList>
#include <QQmlEngine>

//Our includes
#include "cwEquate.h"
#include "cwGlobals.h"

/**
 * QAbstractListModel of cwEquate rows. The cwCavingRegion owns the one list
 * holding every tie in the project, within one node or across nodes. Rows are
 * added/removed as whole equates; individual station handles are not edited in
 * place at this stage (no equate UX yet - that is commit 6).
 *
 * The persistence layer saves the project file off this model's row signals,
 * so a handle added or removed here reaches disk.
 */
class CAVEWHERE_LIB_EXPORT cwEquateModel : public QAbstractListModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(EquateModel)

    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Roles {
        EquateRole = Qt::UserRole + 1,
        StationCountRole
    };
    Q_ENUM(Roles)

    explicit cwEquateModel(QObject* parent = nullptr);
    ~cwEquateModel() override;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    Q_INVOKABLE QModelIndex index(int row, int column = 0, const QModelIndex& parent = QModelIndex()) const override;
    Q_INVOKABLE QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void appendEquate(const cwEquate& equate);
    Q_INVOKABLE void removeAt(int index);
    //! Replaces the equate at \a index in place, one dataChanged() for the row.
    void replaceAt(int index, const cwEquate& equate);
    cwEquate equateAt(int index) const;

    void setEquates(const QList<cwEquate>& equates);
    const QList<cwEquate>& equates() const { return m_equates; }
    int count() const { return m_equates.size(); }

signals:
    void countChanged();

private:
    QList<cwEquate> m_equates;
};

#endif // CWEQUATEMODEL_H
