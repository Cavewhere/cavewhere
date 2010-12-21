#ifndef CWCAVINGREGION_H
#define CWCAVINGREGION_H

//Qt includes
#include <QObject>
#include <QList>

//Our includes
class cwCave;

class cwCavingRegion : public QObject
{
    Q_OBJECT
public:
    explicit cwCavingRegion(QObject *parent = 0);

    int caveCount() const;
    cwCave* cave(int index) const;
    void addCave(cwCave* cave);
    void addCaves(QList<cwCave*> cave);
    void insertCave(int index, cwCave* cave);
    void removeCave(int index);

    int indexOf(cwCave* cave);

signals:
    void insertedCaves(int begin, int end);
    void removedCaves(int begin, int end);

public slots:

protected:
    QList<cwCave*> Caves;

    void insertHelper(int index, cwCave* cave);



};

/**
  \brief Get's the number of caves in the region
  */
inline int cwCavingRegion::caveCount() const {
    return Caves.count();
}

/**
  \brief Get's a cave at index
  */
inline cwCave* cwCavingRegion::cave(int index) const {
    if(index < 0 || index >= Caves.size()) { return NULL; }
    return Caves[index];
}

#endif // CWCAVINGREGION_H
