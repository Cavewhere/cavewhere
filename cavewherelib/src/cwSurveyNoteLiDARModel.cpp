#include "cwSurveyNoteLiDARModel.h"

// CaveWhere
#include "cwProject.h"
#include "cwSaveLoad.h"
#include "cwNoteLiDAR.h"
#include "cwTrip.h"
#include "cwCave.h"

// Qt
#include <QDir>
#include <QFileInfo>
#include <QUrl>
#include <QDebug>

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

    const QDir destinationDirectory = cwSaveLoad::dir(this);

    proj->addFiles(
        glbUrls,
        destinationDirectory,
        [this](QList<QString> relativePaths) {
            if (relativePaths.isEmpty()) {
                return;
            }

            QList<QObject*> newNotes;
            newNotes.reserve(relativePaths.size());

            for (const QString& path : relativePaths) {
                auto* note = new cwNoteLiDAR(this);
                note->setParentTrip(parentTrip());
                note->setParentCave(parentCave());
                note->setFilename(path); // project-relative path
                qDebug() << "lidar note path:" << path;
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
        return note->filename();
    }
    case IconPathRole: {
        // Optional: return a qrc-based generic LiDAR icon (leave empty if none)
        // return QStringLiteral("qrc:/icons/lidar.png");
        return QString();
    }
    case ImageRole: {
        // Not applicable for LiDAR notes (no cwImage). Return empty.
        return QVariant();
    }
    default:
        return QVariant();
    }
}

void cwSurveyNoteLiDARModel::onParentTripChanged()
{
    for (QObject* obj : notes()) {
        if (auto* note = qobject_cast<cwNoteLiDAR*>(obj)) {
            note->setParentTrip(parentTrip());
            if (parentTrip() != nullptr) {
                note->setParentCave(parentTrip()->parentCave());
            }
        }
    }
}

