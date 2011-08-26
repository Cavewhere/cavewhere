#ifndef CWWALLSBOOKDATA_H
#define CWWALLSBOOKDATA_H

//Our includes
#include <cwStationReference.h>
class cwSurveyChunk;
class cwShot;
class cwTeam;
class cwTripCalibration;

//Qt includes
#include <QList>
#include <QObject>
#include <QDate>


class cwWallsBookData : public QObject
{
    Q_OBJECT

    friend class cwWallsImporter;
    friend class cwWallsGlobalData;

public:
    enum ImportType {
        NoImport, //!< Don't import this Book
        Cave, //!< This Book is a cave
        Trip, //!< This Book is a trip
        Structure //!< This is neither a Cave nor a Trip, but a imported Walls Book
    };

    cwWallsBookData(QObject* parent = 0);

    int childBookCount();
    cwWallsBookData* childBook(int index);

    int chunkCount();
    cwSurveyChunk* chunk(int index);

    cwWallsBookData* parentBook() const;

    void setName(QString name);
    QString name() const;

    void setDate(QDate date);
    QDate date() const;

    cwTeam* team() const;
    cwTripCalibration* calibration() const;

    void setImportType(ImportType type);
    ImportType importType() const;
    static QString importTypeToString(ImportType type);

    QList<cwSurveyChunk*> chunks();
    QList<cwWallsBookData*> childBooks();

    int stationCount() const;
    cwStationReference station(int index) const;

    int shotCount() const;
    cwShot* shot(int index) const;
    int indexOfShot(cwShot* shot) const;

signals:
    void nameChanged();
    void importTypeChanged();

private:
    QList<cwSurveyChunk*> Chunks;
    QList<cwWallsBookData*> ChildBooks;
    cwWallsBookData* ParentBook;

    //Mutible elements
    QString Name;
    ImportType Type;

    QDate Date;
    cwTeam* Team;
    cwTripCalibration* Calibration;

    void addChildBook(cwWallsBookData* BookData);
    void addChunk(cwSurveyChunk* chunk);

    void setParentBook(cwWallsBookData* parentBook);

    void clear();

    bool isTrip() const;
};

/**
  \brief Gets the number of child Books
  */
inline int cwWallsBookData::childBookCount() {
    return ChildBooks.size();
}

/**
  \brief Get's the number of chunks
  */
inline int cwWallsBookData::chunkCount() {
    return Chunks.size();
}

/**
  \brief Get's the name of the Book
  */
inline QString cwWallsBookData::name() const {
    return Name;
}

/**
  \brief Get's all the chunks held by the Book
  */
inline QList<cwSurveyChunk*> cwWallsBookData::chunks() {
    return Chunks;
}

/**
  \brief Get's all the child Books held by the this Book
  */
inline QList<cwWallsBookData*> cwWallsBookData::childBooks() {
    return ChildBooks;
}

/**
  \brief Get's the parent Book
  */
inline cwWallsBookData* cwWallsBookData::parentBook() const {
    return ParentBook;
}

/**
  \brief Set's the parent Book
  */
inline void cwWallsBookData::setParentBook(cwWallsBookData* parentBook) {
    ParentBook = parentBook;
}

inline cwWallsBookData::ImportType cwWallsBookData::importType() const {
    return Type;
}

/**
  Sets the date for the Book
  */
inline void cwWallsBookData::setDate(QDate date) {
    Date = date;
}

/**
  Gets the date for the Book
  */
inline QDate cwWallsBookData::date() const {
    return Date;
}

/**
  Gets the survey team for the Book
  */
inline cwTeam* cwWallsBookData::team() const {
    return Team;
}

/**
  Gets the survey's calibration
  */
inline cwTripCalibration* cwWallsBookData::calibration() const {
    return Calibration;
}


#endif // CWWALLSBOOKDATA_H
