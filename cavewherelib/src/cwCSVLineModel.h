#ifndef CWCSVLINEMODEL_H
#define CWCSVLINEMODEL_H

//Qt includes
#include <QAbstractTableModel>
#include <QPointer>
#include <QQmlEngine>

//Our includes
class cwColumnNameModel;
#include "cwGlobals.h"

class CAVEWHERE_LIB_EXPORT cwCSVLineModel : public QAbstractTableModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(CSVLineModel)

    Q_PROPERTY(QList<QStringList> lines READ lines WRITE setLines NOTIFY linesChanged)
    Q_PROPERTY(cwColumnNameModel* columnModel READ columnModel WRITE setColumnModel NOTIFY columnModelChanged)

public:
    cwCSVLineModel(QObject* parent = nullptr);

    QList<QStringList> lines() const;
    void setLines(QList<QStringList> lines);

    cwColumnNameModel* columnModel() const;
    void setColumnModel(cwColumnNameModel* columnModel);

    int rowCount(const QModelIndex& parent = QModelIndex()) const;
    int columnCount(const QModelIndex& parent = QModelIndex()) const;
    QVariant data(const QModelIndex& index, int role) const;

    QHash<int, QByteArray> roleNames() const;

signals:
    void linesChanged();
    void columnModelChanged();

private:
    QList<QStringList> Lines; //!<
    QPointer<cwColumnNameModel> ColumnModel; //!<
};


/**
* Returns all the lines in the CSV file
*/
inline QList<QStringList> cwCSVLineModel::lines() const {
    return Lines;
}

#endif // CWCSVLINEMODEL_H
