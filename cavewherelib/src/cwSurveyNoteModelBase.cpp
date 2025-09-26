#include "cwSurveyNoteModelBase.h"

// CaveWhere
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwProject.h"

// Qt
#include <QDebug>

cwSurveyNoteModelBase::cwSurveyNoteModelBase(QObject* parent)
    : QAbstractListModel(parent),
    m_parentTrip(nullptr),
    m_parentCave(nullptr)
{
}

QHash<int, QByteArray> cwSurveyNoteModelBase::roleNames() const
{
    return {
        { PathRole,       QByteArrayLiteral("path") },
        { IconPathRole,   QByteArrayLiteral("iconPath") },
        { ImageRole,      QByteArrayLiteral("image") },
        { NoteObjectRole, QByteArrayLiteral("noteObject") }
    };
}

int cwSurveyNoteModelBase::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_notes.size();
}

QVariant cwSurveyNoteModelBase::data(const QModelIndex& index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    const int row = index.row();
    if (row < 0 || row >= m_notes.size()) {
        return QVariant();
    }

    // Base only knows about NoteObjectRole; subclasses fill other roles.
    if (role == NoteObjectRole) {
        return QVariant::fromValue(m_notes.at(row));
    }

    return QVariant();
}

QList<QObject*> cwSurveyNoteModelBase::notes() const
{
    return m_notes;
}

void cwSurveyNoteModelBase::removeNote(int index)
{
    const QModelIndex modelIndex = this->index(index);
    if (!modelIndex.isValid()) {
        return;
    }

    beginRemoveRows(QModelIndex(), index, index);
    QObject* note = m_notes.at(index);
    m_notes.removeAt(index);
    note->deleteLater();
    endRemoveRows();
}

bool cwSurveyNoteModelBase::hasNotes() const
{
    return !m_notes.isEmpty();
}

void cwSurveyNoteModelBase::addNotes(QList<QObject*> newNotes)
{
    if (newNotes.isEmpty()) {
        return;
    }

    const int first = m_notes.size();
    const int last = first + newNotes.size() - 1;

    beginInsertRows(QModelIndex(), first, last);
    m_notes.append(newNotes);
    endInsertRows();
}

void cwSurveyNoteModelBase::clearNotes()
{
    beginResetModel();
    for (QObject* note : std::as_const(m_notes)) {
        if (note != nullptr) {
            note->deleteLater();
        }
    }
    m_notes.clear();
    endResetModel();
}

void cwSurveyNoteModelBase::setParentTrip(cwTrip* trip)
{
    if (m_parentTrip != trip) {
        m_parentTrip = trip;
        setParent(m_parentTrip);
        onParentTripChanged();
    }
}

cwTrip* cwSurveyNoteModelBase::parentTrip() const
{
    return m_parentTrip;
}

// void cwSurveyNoteModelBase::setParentCave(cwCave* cave)
// {
//     if (m_parentCave != cave) {
//         m_parentCave = cave;
//         onParentCaveChanged();
//     }
// }

cwCave* cwSurveyNoteModelBase::parentCave() const
{
    return m_parentTrip ? m_parentTrip->parentCave() : nullptr;
}

cwProject* cwSurveyNoteModelBase::project() const
{
    auto parentCave = this->parentCave();
    if (parentCave != nullptr) {
        auto region = parentCave->parentRegion();
        if (region != nullptr) {
            return region->parentProject();
        }
    }
    return nullptr;
}

void cwSurveyNoteModelBase::onParentTripChanged()
{
    // Subclasses push trip into their note type
}

void cwSurveyNoteModelBase::onParentCaveChanged()
{
    // Subclasses push cave into their note type
}
