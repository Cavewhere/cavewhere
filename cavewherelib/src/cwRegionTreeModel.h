/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWREGIONTREEMODEL_H
#define CWREGIONTREEMODEL_H

//Our includes
class cwCavingRegion;
class cwTrip;
class cwCave;
class cwSurveyNode;
class cwNote;
class cwScrap;
class cwNoteLiDAR;
class cwSketch;
#include "cwSurveyNoteModel.h"
#include "cwSurveyNoteLiDARModel.h"
#include "cwSurveyNoteSketchModel.h"
#include "cwGlobals.h"
#include "cwUniqueConnectionChecker.h"

//Qt includes
#include <QAbstractItemModel>
#include <QtGlobal>
#include <QDebug>
#include <QPointer>
#include <QQmlEngine>

#define INVOKE_MEMBER(object,ptrToMember)  ((*object).*(ptrToMember))

/**
 * @brief The cwRegionTreeModel class
 *
 * The regionTreeModel allows for global access to cwRegion via a QAbstractItemModel tree. Currently
 * the tree model supports signaling support for add and removing cwCave, cwTrip, cwNote, cwScrap.
 */
class CAVEWHERE_LIB_EXPORT cwRegionTreeModel : public QAbstractItemModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(RegionTreeModel)

public:
    //! Numbered from Qt::UserRole so they cannot be read as Qt::DisplayRole or
    //! Qt::DecorationRole, which a view asks every model for.
    enum RoleItem {
        TypeRole = Qt::UserRole + 1, //Returns an ItemType
        ObjectRole, //For exctracting the object
    };
    Q_ENUM(RoleItem)

    enum ItemType {
        RegionType,
        NodeType,
        CaveType = NodeType, //!< Deprecated spelling of NodeType, kept until cwCave retires
        TripType,
        NoteType,
        ScrapType,
        NotesType,
        NotesLiDARType,
        NoteLiDARType,
        NotesSketchType,
        SketchType
    };
    Q_ENUM(ItemType)

    enum class TripRows : int {
        NotesModel,
        NotesLiDARModel,
        NotesSketchModel,

        //Add more rows above this
        NumberOfRows
    };

    explicit cwRegionTreeModel(QObject *parent = 0);

    void setCavingRegion(cwCavingRegion* region);
    cwCavingRegion* cavingRegion() const;

    Q_INVOKABLE QModelIndex index ( int row, int column, const QModelIndex & parent ) const;
    QModelIndex index (cwSurveyNode* node) const;
    QModelIndex index (cwTrip* trip) const;
    QModelIndex index (cwNote* note) const;
    QModelIndex index (cwScrap* scrap) const;
    QModelIndex index (cwNoteLiDAR* noteLiDAR) const;
    QModelIndex index (cwSketch* sketch) const;
    QModelIndex index (cwSurveyNoteModel* model) const;
    QModelIndex index (cwSurveyNoteLiDARModel* model) const;
    QModelIndex index (cwSurveyNoteSketchModel* model) const;

    QModelIndex parent ( const QModelIndex & index ) const;
    int rowCount ( const QModelIndex & parent = QModelIndex() ) const;
    int columnCount ( const QModelIndex & parent = QModelIndex() ) const;
    Q_INVOKABLE QVariant data ( const QModelIndex & index, int role) const;

    Q_INVOKABLE cwTrip* trip(const QModelIndex& index) const;
    Q_INVOKABLE cwSurveyNode* node(const QModelIndex& index) const;
    Q_INVOKABLE cwCave* cave(const QModelIndex& index) const;
    Q_INVOKABLE cwNote* note(const QModelIndex& index) const;
    Q_INVOKABLE cwScrap* scrap(const QModelIndex& index) const;
    Q_INVOKABLE cwSurveyNoteModel* notesModel(const QModelIndex& index) const;
    Q_INVOKABLE cwSurveyNoteLiDARModel* notesLiDARModel(const QModelIndex& index) const;
    Q_INVOKABLE cwSurveyNoteSketchModel* notesSketchModel(const QModelIndex& index) const;
    Q_INVOKABLE cwNoteLiDAR* noteLiDAR(const QModelIndex& index) const;
    Q_INVOKABLE cwSketch* sketch(const QModelIndex& index) const;
    Q_INVOKABLE QObject* object(const QModelIndex& index) const;


    Q_INVOKABLE bool isScrap(const QModelIndex& index) const;
    Q_INVOKABLE bool isNote(const QModelIndex& index) const;
    Q_INVOKABLE bool isTrip(const QModelIndex& index) const;
    Q_INVOKABLE bool isNode(const QModelIndex& index) const;
    Q_INVOKABLE bool isCave(const QModelIndex& index) const;
    Q_INVOKABLE bool isRegion(const QModelIndex& index) const;
    Q_INVOKABLE bool isNotes(const QModelIndex& index) const { return index.data(TypeRole).toInt() == NotesType; }
    Q_INVOKABLE bool isNotesLiDAR(const QModelIndex& index) const { return index.data(TypeRole).toInt() == NotesLiDARType; }
    Q_INVOKABLE bool isNoteLiDAR(const QModelIndex& index) const { return index.data(TypeRole).toInt() == NoteLiDARType; }
    Q_INVOKABLE bool isNotesSketch(const QModelIndex& index) const { return index.data(TypeRole).toInt() == NotesSketchType; }
    Q_INVOKABLE bool isSketch(const QModelIndex& index) const { return index.data(TypeRole).toInt() == SketchType; }

    template <typename ReturnType, typename GetFunc>
    QList<ReturnType> objects(const QModelIndex& parent,
                              int begin,
                              int end,
                              GetFunc func) const
    {
        QList<ReturnType> objects;
        for(int i = begin; i <= end; i++) {
            auto rowIndex = index(i, 0, parent);
            ReturnType obj = get<ReturnType>(rowIndex, func);
            if(obj) {
                objects.append(obj);
            }
        }

        return objects;
    }

    template <typename ReturnType, typename GetFunc>
    QList<ReturnType> all(const QModelIndex& parent, GetFunc func) const {
        QList<ReturnType> objects;
        for(int row = 0; row < rowCount(parent); row++) {
            auto rowIndex = index(row, 0, parent);

            //This assert detect infinite recursive calls to all
            Q_ASSERT(rowIndex != QModelIndex());

            auto obj = get<ReturnType>(rowIndex, func);
            if(obj) {
                objects.append(obj);
            }
            objects += all<ReturnType>(rowIndex, func);
        }
        return objects;
    }

    virtual QHash<int, QByteArray> roleNames() const;

signals:

public slots:

private slots:
    void beginInsertNotes(QModelIndex parent, int begin, int end);
    void insertedNotes(QModelIndex parent, int begin, int end);
    void beginRemoveNotes(QModelIndex parent, int begin, int end);
    void removeNotes(QModelIndex parent, int begin, int end);

    void beginInsertScraps(int begin, int end);
    void insertedScraps(int begin, int end);
    void beginRemoveScraps(int begin, int end);
    void removedScraps(int begin, int end);

private:

    QPointer<cwCavingRegion> Region;

    //For debugging connections, this will no-op in release mode
    cwUniqueConnectionChecker m_connectionChecker;

    //! The node an index stands for: the region's root for the invalid index,
    //! the node itself for a node row, nullptr for every other row.
    cwSurveyNode* nodeForIndex(const QModelIndex& index) const;

    //! True when node hangs off this model's region root, so it owns a row here.
    bool isInTree(const cwSurveyNode* node) const;

    //! A node's row under its parent. Child nodes come first, so this is just
    //! the node's place in its parent's child list.
    int rowOf(cwSurveyNode* node) const;

    //! A trip's row under its node, which the node's children are counted ahead of.
    int rowOf(cwTrip* trip) const;

    //! The row a node's first trip takes, its child-node rows coming ahead of it.
    int firstTripRow(const cwSurveyNode* node) const;

    void addNodeConnections(cwSurveyNode* node, bool recursive);
    void removeNodeConnections(cwSurveyNode* parentNode, int beginIndex, int endIndex);

    //! Unwires a node, its descendants, their trips and their notes in one walk
    void removeSubtreeConnections(cwSurveyNode* node);

    void addTripConnections(cwSurveyNode* parentNode, int beginIndex, int endIndex, bool recursive = true);
    void removeTripConnections(cwSurveyNode* parentNode, int beginIndex, int endIndex);

    void addNoteConnections(cwTrip* parentTrip, int beginIndex, int endIndex);
    void removeNoteConnections(cwTrip* parentTrip, int beginIndex, int endIndex);

    void beginInsertNodes(cwSurveyNode* parentNode, int begin, int end);
    void insertedNodes(cwSurveyNode* parentNode, int begin, int end);
    void beginRemoveNodes(cwSurveyNode* parentNode, int begin, int end);

    void beginInsertTrips(cwSurveyNode* parentNode, int begin, int end);
    void insertedTrips(cwSurveyNode* parentNode, int begin, int end);
    void beginRemoveTrips(cwSurveyNode* parentNode, int begin, int end);

    void beginRemoveNotes(cwTrip* parentTrip, int begin, int end);
    void beginRemoveScraps(cwNote* parentNote, int  begin, int end);

    void insertedNotes(cwTrip* parentTrip, int begin, int end);
    void insertedScraps(cwNote* parentNote, int begin, int end);

    //! Announce the child nodes and trips a freshly inserted node already holds
    //! as inserted rows, so a listener discovers rows that were in place before
    //! the connection was made.
    void insertedExistingChildren(cwSurveyNode* node);

    //! The mirror of insertedExistingChildren for removal: take the node's own
    //! child rows out, deepest first.
    void removeChildRows(cwSurveyNode* node);

    template <typename ReturnType, typename GetFunc>
    ReturnType get(const QModelIndex& rowIndex, GetFunc func) const {
        return INVOKE_MEMBER(this, func)(rowIndex); //std::invoke(func, this, rowIndex);
    };

};


/**
  \brief Checks if index is a trip, returns true if it is, false if it isn't
  */
inline bool cwRegionTreeModel::isTrip(const QModelIndex &index) const {
    return index.data(TypeRole).toInt() == TripType;
}

/**
  \brief Checks if index is a survey node, returns true if it is, false if it isn't
  */
inline bool cwRegionTreeModel::isNode(const QModelIndex &index) const {
    return index.data(TypeRole).toInt() == NodeType;
}

/**
  \brief Checks if index is a cave, returns true if it is, false if it isn't

  Every node is a cwCave until the shim retires, but a node that is not one
  answers false here, so isCave() and cave() keep giving the same answer.
  */
inline bool cwRegionTreeModel::isCave(const QModelIndex &index) const {
    return cave(index) != nullptr;
}

/**
  \brief Checks if index is a region, return true if it is, false if it isn't
  */
inline bool cwRegionTreeModel::isRegion(const QModelIndex &index) const {
    return index == QModelIndex();
}

/**
 * @brief cwRegionTreeModel::isScrap
 * @param index
 * @return
 */
inline bool cwRegionTreeModel::isScrap(const QModelIndex &index) const
{
    return index.data(TypeRole).toInt() == ScrapType;
}

/**
 * @brief cwRegionTreeModel::isNote
 * @param index
 * @return
 */
inline bool cwRegionTreeModel::isNote(const QModelIndex &index) const
{
    return index.data(TypeRole).toInt() == NoteType;
}





#endif // CWREGIONTREEMODEL_H
