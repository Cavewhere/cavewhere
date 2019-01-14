//Our includes
#include "cwCSVLineModel.h"
#include "cwColumnNameModel.h"

cwCSVLineModel::cwCSVLineModel(QObject *parent) :
    QAbstractTableModel(parent)
{

}

/**
* Lets the CSV file lines that have already been seperated into StringList
*/
void cwCSVLineModel::setLines(QList<QStringList> lines) {
    if(Lines != lines) {
        beginResetModel();
        Lines = lines;
        endResetModel();
        emit linesChanged();
    }
}

/**
* Sets the column model that represents the columns in the text file
*/
void cwCSVLineModel::setColumnModel(cwColumnNameModel* columnModel) {
    if(ColumnModel != columnModel) {

        if(!ColumnModel.isNull()) {
            disconnect(ColumnModel.data(), nullptr, this, nullptr);
        }

        beginResetModel();
        ColumnModel = columnModel;

        if(!ColumnModel.isNull()) {
            connect(ColumnModel.data(), &QAbstractItemModel::modelAboutToBeReset,
                    this, [this](){beginResetModel();});
            connect(ColumnModel.data(), &QAbstractItemModel::modelReset,
                    this, [this](){endResetModel();});
            connect(ColumnModel.data(), &QAbstractItemModel::rowsAboutToBeRemoved,
                    this, [this](QModelIndex, int begin, int end) { beginRemoveColumns(QModelIndex(), begin, end); });
            connect(ColumnModel.data(), &QAbstractItemModel::rowsRemoved,
                    this, [this](QModelIndex, int, int) { endRemoveColumns(); });
            connect(ColumnModel.data(), &QAbstractItemModel::rowsAboutToBeInserted,
                    this, [this](QModelIndex, int begin, int end) { beginInsertColumns(QModelIndex(), begin, end); });
            connect(ColumnModel.data(), &QAbstractItemModel::rowsInserted,
                    this, [this](QModelIndex, int, int) { endInsertColumns(); });
        }

        endResetModel();



        emit columnModelChanged();
    }
}

/**
 * Returns the number of lines in the csv file.
 *
 * This will have an extra line because TableView doesn't support headers, so the first
 * line is the header
 */
int cwCSVLineModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);

    if(ColumnModel.isNull()) {
        return 0;
    }

    return Lines.size() + 1; //Plus one for the header since 5.12 TableView doesn't support headerSection
}

/**
 * Returns the number of columns in the model
 */
int cwCSVLineModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);

    if(!ColumnModel.isNull()) {
        return ColumnModel->size();
    }
    return 0;
}

/**
 * Returns the data for each cell in the CSV file.  The first line will be the header
 */
QVariant cwCSVLineModel::data(const QModelIndex &index, int role) const
{
    if(!index.isValid()) {
        return QVariant();
    }

    int i = index.row() - 1;

    if(i < 0) {
        //This is header code hack because TableView doesn't support header
        if(!ColumnModel.isNull()) {
            switch(role) {
            case Qt::DisplayRole:
                return ColumnModel->at(index.column()).name();
            default:
                return QVariant();
            }
        }
        return QVariant();
    }

    auto line = Lines.at(i);
    if(index.column() >= line.size()) {
        return QVariant();
    }
    auto cell = line.at(index.column());

    switch(role) {
    case Qt::DisplayRole:
        return cell;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> cwCSVLineModel::roleNames() const
{
    return {
        {Qt::DisplayRole, "displayRole"}
    };
}

/**
* Returns the column model that stores the columns of the use columns in CSV file
*/
cwColumnNameModel* cwCSVLineModel::columnModel() const {
    return ColumnModel;
}
