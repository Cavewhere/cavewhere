#ifndef CWSURVERYCHUNKGROUP_H
#define CWSURVERYCHUNKGROUP_H

//Our includes
class cwSurveyChunk;
class cwCave;
class cwStationReference;

//Qt include
#include <QObject>
#include <QList>
#include <QAbstractListModel>
#include <QWeakPointer>
#include <QDate>

class cwTrip : public QObject
{
    Q_OBJECT
public:
    explicit cwTrip(QObject *parent = 0);
    cwTrip(const cwTrip& object);
    cwTrip& operator=(const cwTrip& object);

    QString name() const;
    void setName(QString name);

    QDate date() const;
    void setDate(QDate date);

    void removeChunks(int begin, int end);
    void insertChunk(int row, cwSurveyChunk* chunk);
    void addChunk(cwSurveyChunk* chunk);

    int numberOfChunks() const;
    cwSurveyChunk* chunk(int i) const;
    QList<cwSurveyChunk*> chunks() const;

    void setParentCave(cwCave* parentCave);
    cwCave* parentCave();

    QList< cwStationReference* > uniqueStations() const;

    //Stats
    int numberOfStations() const;

signals:
    void nameChanged(QString name);
    void dateChanged(QDate date);
    void chunksInserted(int begin, int end);
    void chunksRemoved(int begin, int end);

public slots:
    void setChucks(QList<cwSurveyChunk*> chunks);


protected:
    QList<cwSurveyChunk*> Chunks;
    QString Name;
    QDate Date;
    cwCave* ParentCave;

    //Units
    cw

private:
    void Copy(const cwTrip& object);

};

/**
  \brief Get's the name of the survey trip
  */
inline QString cwTrip::name() const {
    return Name;
}

/**
  Gets the date of the survey trip
  */
inline QDate cwTrip::date() const {
    return Date;
}

/**
  \brief Parent's cave
  */
inline cwCave* cwTrip::parentCave() {
    return ParentCave;
}

#endif // CWSURVERYCHUNKGROUP_H
