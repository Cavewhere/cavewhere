/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#ifndef CWLINKBARMODEL_H
#define CWLINKBARMODEL_H

//Our includes
#include <QObject>
#include <QAbstractListModel>
#include <QQmlEngine>

/**
 * @brief The cwLinkBarModel class
 *
 * This class takes an address from the cwPageSelectionModel and creates a model
 * by splitting the address by the cwPageSelectionModel::seperator(), usually "/".
 * This class helps support the LinkBar.qml
 */
class cwLinkBarModel : public QAbstractListModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(LinkBarModel)

    Q_PROPERTY(QString address READ address WRITE setAddress NOTIFY addressChanged)

public:
    enum Role {
        FullPath,
        Name
    };
    Q_ENUM(Role)

    cwLinkBarModel();
    ~cwLinkBarModel();

    QHash<int, QByteArray> roleNames() const;
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role) const;

    QString address() const;
    void setAddress(QString address);

signals:
    void addressChanged();

private:

    QString Address; //!<
    QStringList Names;

    void updateNames();
    QString fullPath(int index) const;

};


/**
* @brief cwLinkBarModel::address
* @return The adddress of the link bar model
*/
inline QString cwLinkBarModel::address() const {
    return Address;
}

#endif // CWLINKBARMODEL_H
