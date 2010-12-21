#ifndef CWSURVERYCHUNKGROUP_H
#define CWSURVERYCHUNKGROUP_H

//Our includes
class cwSurveyChunk;

//Qt include
#include <QObject>
#include <QList>
#include <QAbstractListModel>

class cwSurveyTrip : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles {
        ChunkRole = Qt::UserRole + 1,
    };

    explicit cwSurveyTrip(QObject *parent = 0);

    QString name() const;
    void setName(QString name);

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role) const;

    bool insertRows ( int row, int count, const QModelIndex & parent = QModelIndex() );

    void insertRow(int row, cwSurveyChunk* chunk);
    void addChunk(cwSurveyChunk* chunk);

    QList<cwSurveyChunk*> chunks() const;


signals:
    void dataChanged();
    void nameChanged(QString name);

public slots:
    void setChucks(QList<cwSurveyChunk*> chunks);


protected:
    QList<cwSurveyChunk*> Chunks;
    QString Name;
};

/**
  \brief Get's the name of the survey trip
  */
inline QString cwSurveyTrip::name() const {
    return Name;
}

#endif // CWSURVERYCHUNKGROUP_H
