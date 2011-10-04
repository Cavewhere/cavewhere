#ifndef CWSCRAPMODEL_H
#define CWSCRAPMODEL_H

#include <QAbstractListModel>

class cwScrapModel : public QAbstractListModel
{
    Q_OBJECT
public:
    explicit cwScrapModel(QObject *parent = 0);

    void rowCount(const QModelIndex &parent) const;
    QVariant data(const QModelIndex &index, int role) const;


signals:

public slots:

};

Q_DECLARE_METATYPE(cwScrapModel*)

#endif // CWSCRAPMODEL_H
