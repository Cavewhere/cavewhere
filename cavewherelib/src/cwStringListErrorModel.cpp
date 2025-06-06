/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwStringListErrorModel.h"

//Qt includes
#include <QIcon>
#include <QRegularExpression>
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
        if (error.contains(QRegularExpression("^error:", QRegularExpression::CaseInsensitiveOption))) {
            return QIcon(":icons/svg/stopSignError.svg");
        } else if (error.contains(QRegularExpression("^warning:", QRegularExpression::CaseInsensitiveOption))) {
            return QIcon(":icons/svg/warning.svg");
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
