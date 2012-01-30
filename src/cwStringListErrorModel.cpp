//Our includes
#include "cwStringListErrorModel.h"

//Qt includes
#include <QIcon>
#include <QDebug>

cwStringListErrorModel::cwStringListErrorModel(QObject *parent) :
    QAbstractListModel(parent)
{
}

int cwStringListErrorModel::rowCount(const QModelIndex&) const {
    return ErrorData.size();

}

QVariant  cwStringListErrorModel::data(const QModelIndex &index, int role) const {
    if(!index.isValid()) { return QVariant(); }

    QString error = ErrorData.at(index.row());
    switch(role) {
    case Qt::DisplayRole:
        return error;
    case Qt::DecorationRole: {
        if(error.contains(QRegExp("^error:", Qt::CaseInsensitive))) {
            return QIcon(":icons/stopSignError.png");
        } else if(error.contains(QRegExp("^warning:", Qt::CaseInsensitive))) {
            return QIcon(":icons/warning.png");
        }
        return QVariant();
    }

    default:
        return QVariant();
    }

}

/**
  Sets all the errors for this model
  */
void cwStringListErrorModel::setStringList(QStringList list) {
    beginResetModel();
    ErrorData = list;
    endResetModel();
}
