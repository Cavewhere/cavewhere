#ifndef CWCOLUMNNAMEMODEL_H
#define CWCOLUMNNAMEMODEL_H

//Qt includes
#include <QAbstractListModel>

//Our inculdes
#include "cwGlobals.h"

/**
 * @brief The cwColumnNameModel class
 *
 * Column names store a list of id's and column names. This is useful for listing
 * column for CSVImporter, but could be useful for other column orginization
 */
class CAVEWHERE_LIB_EXPORT cwColumnNameModel : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(QVector<Column> columns READ columns WRITE setColumns NOTIFY columnsChanged)

public:
    enum Role {
        NameRole,
        IdRole
    };

    class Column {
    public:
        Column() {}
        Column(const QString& name, int id) :
            Name(name),
            Id(id)
        {

        }

        QString Name;
        int Id = -1;
    };

    cwColumnNameModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent) const;
    QVariant data(const QModelIndex& index, int role) const;

    QHash<int, QByteArray> roleNames() const;

    void setColumns(const QVector<Column> columns);
    QVector<Column> columns() const;

signals:
    void columnsChanged();

private:
    QVector<Column> Columns;
};

inline QVector<cwColumnNameModel::Column> cwColumnNameModel::columns() const
{
    return Columns;
}

#endif // CWCOLUMNNAMEMODEL_H
