#include "cwSurveyNoteLiDARModel.h"

// CaveWhere
#include "cwProject.h"
#include "cwNoteLiDAR.h"
#include "cwCave.h"
#include "cwTrip.h"

// Qt
#include <QDir>
#include <QFileInfo>
#include <QUrl>
#include <QDebug>
#include <QImageReader>

cwSurveyNoteLiDARModel::cwSurveyNoteLiDARModel(QObject* parent)
    : cwSurveyNoteModelBase(parent)
{
}

void cwSurveyNoteLiDARModel::addFromFiles(QList<QUrl> files)
{
    QList<QUrl> glbUrls;
    glbUrls.reserve(files.size());
    for (const QUrl& u : files) {
        const QFileInfo fi(u.toLocalFile());
        if (fi.suffix().compare(QStringLiteral("glb"), Qt::CaseInsensitive) == 0) {
            glbUrls.append(u);
        }
    }

    if (glbUrls.isEmpty()) {
        qWarning() << "No GLB files provided to cwSurveyNoteLiDARModel::addFromFiles";
        return;
    }

    auto* proj = project();
    if (proj == nullptr) {
        qWarning() << "Project not found for cwSurveyNoteLiDARModel::addFromFiles";
        return;
    }

    const QDir destinationDirectory = proj->notesDir(this);

    proj->addFiles(
        glbUrls,
        destinationDirectory,
        [this](QList<QString> relativePaths) {
            if (relativePaths.isEmpty()) {
                return;
            }

            QList<cwNoteLiDAR*> newNotes;
            newNotes.reserve(relativePaths.size());

            for (const QString& path : relativePaths) {
                auto* note = new cwNoteLiDAR(this);
                note->setParentTrip(parentTrip());
                note->setName(QFileInfo(path).fileName());
                note->setFilename(QFileInfo(path).fileName()); // store filename only; runtime resolution happens via noteLiDARRelativePath
                newNotes.append(note);
            }

            addNotes(newNotes);
        }
        );
}

QVariant cwSurveyNoteLiDARModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    // Base handles NoteObjectRole
    if (role == NoteObjectRole) {
        return cwSurveyNoteModelBase::data(index, role);
    }

    const auto* note = qobject_cast<cwNoteLiDAR*>(notes().value(index.row()));
    if (note == nullptr) {
        return QVariant();
    }

    switch (role) {
    case PathRole: {
        // For LiDAR, expose the stored project-relative filename (.glb)
        auto* proj = project();
        if (proj == nullptr) {
            return QVariant();
        }
        return proj->absolutePath(note, note->filename());
    }
    case IconPathRole: {
        return note->iconImagePath();
    }
    case ImageRole: {
        // Not applicable for LiDAR notes (no cwImage). Return empty.
        return QVariant();
    }
    default:
        return QVariant();
    }
}

void cwSurveyNoteLiDARModel::addNotes(const QList<cwNoteLiDAR *> lidarNotes) {
    connectNotes(lidarNotes);
    addNotesHelper(lidarNotes);
}

void cwSurveyNoteLiDARModel::setData(const cwSurveyNoteLiDARModelData &data)
{
    setDataHelper<cwSurveyNoteLiDARModelData, cwNoteLiDAR>(data);
    connectNotes(notes());
}

cwSurveyNoteLiDARModelData cwSurveyNoteLiDARModel::data() const
{
    return dataHelper<cwSurveyNoteLiDARModelData, cwNoteLiDAR>();
}

void cwSurveyNoteLiDARModel::onParentTripChanged()
{
    const auto notes = this->notes();
    for (QObject* obj : notes) {
        if (auto* note = qobject_cast<cwNoteLiDAR*>(obj)) {
            note->setParentTrip(parentTrip());
        }
    }

    auto* proj = project();
    if (m_project != proj) {
        if (m_pathReadyConnection) {
            disconnect(m_pathReadyConnection);
        }
        m_project = proj;
        if (m_project != nullptr) {
            m_pathReadyConnection = connect(m_project, &cwProject::objectPathReady, this, [this](QObject* object) {
                if (object == nullptr) {
                    return;
                }

                auto* trip = parentTrip();
                if (trip == nullptr) {
                    return;
                }

                auto emitForNote = [this](cwNoteLiDAR* note) {
                    const QList<QObject*> objNotes = this->notes();
                    const int row = objNotes.indexOf(note);
                    if (row < 0) {
                        return;
                    }
                    const QModelIndex modelIndex = index(row, 0);
                    if (modelIndex.isValid()) {
                        emit dataChanged(modelIndex, modelIndex, {PathRole, IconPathRole, ImageRole});
                    }
                };

                if (auto* note = qobject_cast<cwNoteLiDAR*>(object)) {
                    if (note->parentTrip() == trip) {
                        emitForNote(note);
                    }
                    return;
                }

                if (auto* tripObject = qobject_cast<cwTrip*>(object)) {
                    if (tripObject != trip) {
                        return;
                    }
                    const QList<QObject*> objNotes = this->notes();
                    for (QObject* obj : objNotes) {
                        if (auto* note = qobject_cast<cwNoteLiDAR*>(obj)) {
                            emitForNote(note);
                        }
                    }
                    return;
                }

                if (auto* caveObject = qobject_cast<cwCave*>(object)) {
                    if (trip->parentCave() != caveObject) {
                        return;
                    }
                    const QList<QObject*> objNotes = this->notes();
                    for (QObject* obj : objNotes) {
                        if (auto* note = qobject_cast<cwNoteLiDAR*>(obj)) {
                            emitForNote(note);
                        }
                    }
                }
            });
        }
    }
}
