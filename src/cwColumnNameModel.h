#ifndef CWCOLUMNNAMEMODEL_H
#define CWCOLUMNNAMEMODEL_H

//Qt includes
#include <QAbstractListModel>

//Our inculdes
#include "cwGlobals.h"
#include "cwColumnName.h"
#include "QQmlGadgetListModel.h"

/**
 * @brief The cwColumnNameModel class
 *
 * Column names store a list of id's and column names. This is useful for listing
 * column for CSVImporter, but could be useful for other column orginization
 */
class CAVEWHERE_LIB_EXPORT cwColumnNameModel : public QQmlGadgetListModel<cwColumnName>
{
    Q_OBJECT

public:
    cwColumnNameModel(QObject* parent = nullptr);
};

#endif // CWCOLUMNNAMEMODEL_H
