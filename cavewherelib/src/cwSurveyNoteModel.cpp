#include "cwSurveyNoteModel.h"

// CaveWhere
#include "cwNote.h"
#include "cwImage.h"
#include "cwImageProvider.h"
#include "cwProject.h"
#include "cwTrip.h"
#include "cwCave.h"

// Qt
#include <QDir>
#include <QFileInfo>
#include <QDebug>

cwSurveyNoteModel::cwSurveyNoteModel(QObject* parent)
    : cwSurveyNoteModelBase(parent)
{
}

QVariant cwSurveyNoteModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    // Always allow base to serve NoteObjectRole
    if (role == NoteObjectRole) {
        return cwSurveyNoteModelBase::data(index, role);
    }

    const auto* note = qobject_cast<cwNote*>(notes().value(index.row()));
    if (note == nullptr) {
        return QVariant();
    }

    auto* project = this->project();
    if (project == nullptr) {
        return QVariant();
    }

    switch (role) {
    case IconPathRole:
        //Icon path is just cached
    case PathRole: {
        const QString imagePath = note->image().path();
        Q_ASSERT(!QFileInfo(imagePath).isAbsolute()); //This should be a relative path
        const QString absolutePath = project->absolutePath(note, imagePath);
        return cwImageProvider::imageUrl(note->image(), absolutePath);
    }
    case ImageRole: {
        return QVariant::fromValue(project->absolutePathNoteImage(note));
    }
    default:
        return QVariant();
    }
}

void cwSurveyNoteModel::addFromFiles(QList<QUrl> files)
{
    const QList<QUrl> imageUrls = files;
    if (imageUrls.isEmpty()) {
        qWarning() << "No supported image/PDF files provided to cwSurveyNoteModel::addFromFiles";
        return;
    }

    auto* project = this->project();
    if (project == nullptr) {
        qWarning() << "Project not found for cwSurveyNoteModel::addFromFiles";
        return;
    }

    const QDir destinationDirectory = project->notesDir(this);

    project->addImages(
        imageUrls,
        destinationDirectory,
        [this](QList<cwImage> newImages) {
            addNotesWithNewImages(newImages);
        }
        );
}

QList<cwNote *> cwSurveyNoteModel::notes() const
{
    auto baseNotes = cwSurveyNoteModelBase::notes();
    QList<cwNote*> notes;
    notes.reserve(baseNotes.size());
    for(const auto note : std::as_const(baseNotes)) {
        notes.append(static_cast<cwNote*>(note));
    }
    return notes;
}

void cwSurveyNoteModel::addNotes(const QList<cwNote *>& notes)
{
    addNotesHelper(notes);
}

void cwSurveyNoteModel::setData(const cwSurveyNoteModelData &data)
{
    setDataHelper<cwSurveyNoteModelData, cwNote>(data);
}

cwSurveyNoteModelData cwSurveyNoteModel::data() const
{
    return dataHelper<cwSurveyNoteModelData, cwNote>();
}

void cwSurveyNoteModel::onParentTripChanged()
{
    const auto notes = this->notes();
    for (QObject* obj : notes) {
        if (auto* note = qobject_cast<cwNote*>(obj)) {
            note->setParentTrip(parentTrip());
        }
    }

    auto* project = this->project();
    if (m_project != project) {
        if (m_pathReadyConnection) {
            disconnect(m_pathReadyConnection);
        }
        m_project = project;
        if (m_project != nullptr) {
            m_pathReadyConnection = connect(m_project, &cwProject::objectPathReady, this, [this](QObject* object) {
                if (object == nullptr) {
                    return;
                }

                auto* trip = parentTrip();
                if (trip == nullptr) {
                    return;
                }

                auto emitForNote = [this](cwNote* note) {
                    const QList<cwNote*> objNotes = this->notes();
                    const int row = objNotes.indexOf(note);
                    if (row < 0) {
                        return;
                    }
                    const QModelIndex modelIndex = index(row, 0);
                    if (modelIndex.isValid()) {
                        emit dataChanged(modelIndex, modelIndex, {PathRole, IconPathRole, ImageRole});
                    }
                };

                if (auto* note = qobject_cast<cwNote*>(object)) {
                    if (note->parentTrip() == trip) {
                        emitForNote(note);
                    }
                    return;
                }

                if (auto* tripObject = qobject_cast<cwTrip*>(object)) {
                    if (tripObject != trip) {
                        return;
                    }
                    const QList<cwNote*> objNotes = this->notes();
                    for (auto note : objNotes) {
                        emitForNote(note);
                    }
                    return;
                }

                if (auto* caveObject = qobject_cast<cwCave*>(object)) {
                    if (trip->parentCave() != caveObject) {
                        return;
                    }
                    const QList<cwNote*> objNotes = this->notes();
                    for (auto note : objNotes) {
                        emitForNote(note);
                    }
                }
            });
        }
    }
}


QList<cwNote*> cwSurveyNoteModel::validateNoteImages(QList<cwNote*> noteList) const
{
    QList<cwNote*> valid;
    valid.reserve(noteList.size());
    for (cwNote* n : noteList) {
        if (n != nullptr && n->image().isValid()) {
            valid.append(n);
        } else if (n != nullptr) {
            qWarning() << "Note image not valid, removing";
            n->deleteLater();
        }
    }
    return valid;
}

void cwSurveyNoteModel::addNotesWithNewImages(QList<cwImage> images)
{
    if (images.isEmpty()) {
        return;
    }

    QList<QObject*> newNotes;
    newNotes.reserve(images.size());

    for (const cwImage& image : images) {
        auto* note = new cwNote(this);
        const QString baseName = QFileInfo(image.path()).fileName();
        if (image.page() >= 0) {
            note->setName(QStringLiteral("%1 (Page %2)")
                              .arg(baseName)
                              .arg(image.page() + 1));
        } else {
            note->setName(baseName);
        }
        note->setImage(image);
        note->setParentTrip(parentTrip());
        newNotes.append(note);
    }

    // Validate by image validity
    QList<cwNote*> typed;
    typed.reserve(newNotes.size());
    for (QObject* o : newNotes) {
        typed.append(static_cast<cwNote*>(o));
    }
    const QList<cwNote*> valid = validateNoteImages(typed);

    QList<QObject*> validObjs;
    validObjs.reserve(valid.size());
    for (cwNote* n : valid) {
        validObjs.append(n);
    }

    cwSurveyNoteModelBase::addNotes(validObjs);
}

QString cwSurveyNoteModel::imagePathString()
{
    return QLatin1String("image://") + cwImageProvider::name() + QLatin1String("/%1");
}
