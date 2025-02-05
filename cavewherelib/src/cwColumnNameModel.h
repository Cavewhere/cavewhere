#ifndef CWCOLUMNNAMEMODEL_H
#define CWCOLUMNNAMEMODEL_H

//Qt includes
#include <QAbstractListModel>
#include <QQmlEngine>

//Our inculdes
#include "cwGlobals.h"
#include "cwColumnName.h"

/**
 * @brief The cwColumnNameModel class
 *
 * Column names store a list of id's and column names. This is useful for listing
 * column for CSVImporter, but could be useful for other column orginization
 */
class CAVEWHERE_LIB_EXPORT cwColumnNameModel : public QAbstractListModel //public QQmlGadgetListModel<cwColumnName>
{
    Q_OBJECT
    QML_NAMED_ELEMENT(ColumnNameModel)

public:
    enum class ColumnRoles : int {
        Name = Qt::UserRole + 1,
        ColumnId
    };

    cwColumnNameModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex &parent) const;
    QVariant data(const QModelIndex &index, int role) const;
    QHash<int, QByteArray> roleNames() const;

    int size() const { return m_columnNames.size(); }
    QList<cwColumnName> toList() const { return m_columnNames; }
    void append(const cwColumnName& columnName);
    void append(const QList<cwColumnName>& columnNames);
    cwColumnName at(int index) const { return m_columnNames.at(index); }
    void insert(int index, const cwColumnName& columnName);
    void insert(int index, const QList<cwColumnName>& columnNames);
    void remove(int index);
    void clear();

private:
    QList<cwColumnName> m_columnNames;
};

#endif // CWCOLUMNNAMEMODEL_H
