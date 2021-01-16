#ifndef CWGENERICTABLEMODEL_H
#define CWGENERICTABLEMODEL_H

//Qt includes
#include <QAbstractTableModel>

//Our includes
#include "cwGlobals.h"

class CAVEWHERE_LIB_EXPORT cwGenericTableModelBase : public QAbstractTableModel {
    Q_OBJECT

public:
    cwGenericTableModelBase(QObject* parent = nullptr) : QAbstractTableModel(parent) {}

    Q_INVOKABLE virtual void append(const QVariant& row) = 0;
    Q_INVOKABLE virtual void remove(int row) = 0;
    Q_INVOKABLE virtual QVariant rowAt(int row) const = 0;
    Q_INVOKABLE virtual void setRow(int row, const QVariant& value) = 0;
    Q_INVOKABLE virtual bool isEmpty() const = 0;
    Q_INVOKABLE virtual int column(const QString& columnName) const = 0;
    Q_INVOKABLE virtual void clear() = 0;
};

template <typename T>
class cwGenericTableModel : public cwGenericTableModelBase
{

protected:
    class Column {
    public:
        Column() = default;
        Column(const QString& name) :
            Name(name)
        {}

        template <typename Functor>
        void registerDataRole(int role, Functor&& f) {
            dataFunctions.insert(role, f);
        }

        template <typename Functor>
        void registerSetDataRole(int role, Functor&& f) {
            setDataFunctions.insert(role, f);
        }

        QVariant data(const QModelIndex& index, const T& row, int role) const {
            if(dataFunctions.contains(role)) {
                return dataFunctions.value(role)(index, row);
            }
            return QVariant();
        }

        bool setData(const QModelIndex& index, T& row, const QVariant& value, int role) {
            if(setDataFunctions.contains(role)) {
                return setDataFunctions.value(role)(index, row, value);
            }
            return false;
        }

        QString name() const { return Name; }

        private:
        QString Name;
        QHash<int, std::function<QVariant (const QModelIndex& index, const T& row)>> dataFunctions;
        QHash<int, std::function<bool (const QModelIndex& index, T& row, const QVariant& value)>> setDataFunctions;
    };

public:
    cwGenericTableModel(QObject* parent = nullptr) :
        cwGenericTableModelBase(parent)
    {

    }
    ~cwGenericTableModel() {
        for(auto column : Columns) {
            delete column;
        }
    }

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const {
        if(!index.isValid()) {
            return QVariant();
        }

        const auto& row = Rows.at(index.row());
        auto column = Columns.at(index.column());

        return column->data(index, row, role);
    }

    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::DisplayRole) {
        if(!index.isValid()) {
            return false;
        }

        auto& row = Rows[index.row()];
        auto column = Columns.at(index.column());

        return column->setData(index, row, value, role);
    }

    int rowCount(const QModelIndex &parent = QModelIndex()) const {
        Q_UNUSED(parent);
        return Rows.size();
    }

    int columnCount(const QModelIndex &parent = QModelIndex()) const {
        Q_UNUSED(parent);
        return Columns.size();
    }

    QVariant headerData(int section, Qt::Orientation orientation = Qt::Horizontal, int role = Qt::DisplayRole) const {
        if(orientation == Qt::Horizontal) {
            if(role == Qt::DisplayRole) {
                if(section < columnCount() && section >= 0) {
                    return Columns.at(section)->name();
                }
            }
        }

        return QVariant();
    }

    void append(const QVariant& row) {
        if(row.canConvert<T>()) {
            append(row.value<T>());
        }
    }

    void append(const T& row) {
        int lastRow = rowCount();
        beginInsertRows(QModelIndex(), lastRow, lastRow);
        Rows.append(row);
        endInsertRows();
    }

    void append(const QList<T>& rows) {
        if(rows.isEmpty()) {
            return;
        }

        int lastRow = rowCount();
        beginInsertRows(QModelIndex(), lastRow, lastRow + rows.size() - 1);
        Rows.append(rows);
        endInsertRows();
    }

    void remove(int row) {
        beginRemoveRows(QModelIndex(), row, row);
        Rows.removeAt(row);
        endRemoveRows();
    }

    const T& row(int row) const {
        return Rows.at(row);
    }

    QVariant rowAt(int row) const {
        return QVariant::fromValue(this->row(row));
    }

    void setRow(int row, const T& value) {
        Rows[row] = value;
        auto left = index(row, 0);
        auto right = index(row, columnCount() - 1);
        dataChanged(left, right, { /* empty for all roles */ });
    }

    void setRow(int row, const QVariant& value) {
        if(value.canConvert<T>()) {
            setRow(row, value.value<T>());
        }
    }

    QList<T> toList() const {
        return Rows;
    }

    Q_INVOKABLE bool isEmpty() const {
        return Rows.isEmpty();
    }

    T firstRow() const {
        return Rows.first();
    }

    int column(const QString& columnName) const {
        auto iter = std::find_if(Columns.begin(), Columns.end(),
                     [columnName](const Column* column)
        {
            return column->name() == columnName;
        });

        if(iter != Columns.end()) {
            return std::distance(Columns.begin(), iter);
        }

        return -1;
    }

    void clear() {
        beginResetModel();
        Rows.clear();
        endResetModel();
    }

protected:    
    Column* addColumn(QString columnName) {
        auto column = new Column(columnName);
        Columns.append(column);
        return column;
    }

    int column(Column* column) const {
        return Columns.indexOf(column);
    }

private:
    //Column, role
    QList <T> Rows;
    QList<Column*> Columns;
};

#endif // CWGENERICTABLEMODEL_H
