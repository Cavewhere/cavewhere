#ifndef CWCAVINGREGION_H
#define CWCAVINGREGION_H

//Qt includes
#include <QObject>
#include <QList>
#include <QUndoCommand>
//Our includes
class cwCave;

class cwCavingRegion : public QObject
{
    Q_OBJECT

public:
    explicit cwCavingRegion(QObject *parent = 0);
    cwCavingRegion(const cwCavingRegion& object);
    cwCavingRegion& operator=(const cwCavingRegion& object);

    Q_INVOKABLE int caveCount() const;
    Q_INVOKABLE cwCave* cave(int index) const;

    Q_INVOKABLE void addCave(cwCave* cave = NULL);
    void addCaves(QList<cwCave*> cave);
    void insertCave(int index, cwCave* cave);
    Q_INVOKABLE void removeCave(int index);
    void removeCaves(int beginIndex, int endIndex);

    int indexOf(cwCave* cave);

signals:
    void beginInsertCaves(int begin, int end);
    void insertedCaves(int begin, int end);

    void beginRemoveCaves(int begin, int end);
    void removedCaves(int begin, int end);

public slots:

protected:
    QList<cwCave*> Caves;

private:
    cwCavingRegion& copy(const cwCavingRegion& object);

    void unparentCave(cwCave* cave);
    void addCaveHelper();

    class InsertRemoveCave : public QUndoCommand {
    public:
        InsertRemoveCave(cwCavingRegion* region, int beginIndex, int endIndex);

    protected:
        void insertCaves();
        void removeCaves();

        QList<cwCave*> Caves;
    private:
        cwCavingRegion* Region;
        int BeginIndex;
        int EndIndex;
    };

    ////////////////////// Undo Redo commands ///////////////////////////////////
    class InsertCaveCommand : public InsertRemoveCave {

    public:
        InsertCaveCommand(cwCavingRegion* parentRegion, cwCave* cave, int index);
        InsertCaveCommand(cwCavingRegion* parentRegion, QList<cwCave*> cave, int index);
        virtual ~InsertCaveCommand();
        virtual void redo();
        virtual void undo();
    };

    class RemoveCaveCommand : public InsertRemoveCave {
    public:
        RemoveCaveCommand(cwCavingRegion* region, int beginIndex, int endIndex);
        virtual void redo();
        virtual void undo();
    };



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
