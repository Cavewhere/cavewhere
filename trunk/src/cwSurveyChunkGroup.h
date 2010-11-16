#ifndef CWSURVERYCHUNKGROUP_H
#define CWSURVERYCHUNKGROUP_H

//Our includes
class cwSurveyChunk;

//Qt include
#include <QObject>
#include <QList>
#include <QAbstractListModel>

class cwSurveyChunkGroup : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles {
        ChunkRole = Qt::UserRole + 1,
    };


    explicit cwSurveyChunkGroup(QObject *parent = 0);

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role) const;

    bool insertRows ( int row, int count, const QModelIndex & parent = QModelIndex() );

    void insertRow(int row, cwSurveyChunk* chunk);

    QList<cwSurveyChunk*> chunks() const;


signals:
    void dataChanged();

public slots:
    void setChucks(QList<cwSurveyChunk*> chunks);


protected:
    QList<cwSurveyChunk*> Chunks;

};

#endif // CWSURVERYCHUNKGROUP_H
