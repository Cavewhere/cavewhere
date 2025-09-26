#include "cwSurveyNoteModel.h"

// CaveWhere
#include "cwNote.h"
#include "cwImage.h"
#include "cwImageProvider.h"
#include "cwPDFConverter.h"
#include "cwSaveLoad.h"
#include "cwProject.h"
#include "cwTrip.h"
#include "cwCave.h"

// Qt
#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QSet>
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

    switch (role) {
    case PathRole: {
        const cwImage image = note->image();
        // present via image provider URL to keep QML bindings consistent
        return cwImageProvider::imageUrl(image.path());
    }
    case IconPathRole: {
        const cwImage image = note->image();
        // TODO: replace with cached icon path when available
        return cwImageProvider::imageUrl(image.path());
    }
    case ImageRole: {
        return QVariant::fromValue(note->image());
    }
    default:
        return QVariant();
    }
}

void cwSurveyNoteModel::addFromFiles(QList<QUrl> files)
{
    QSet<QString> supportedSuffixes;
    for (const QByteArray& fmt : QImageReader::supportedImageFormats()) {
        supportedSuffixes.insert(QString::fromLatin1(fmt).toLower());
    }

    auto isPdfPath = [](const QString& path) {
        if (!cwPDFConverter::isSupported()) {
            return false;
        }
        const QFileInfo fi(path);
        return fi.suffix().compare(QStringLiteral("pdf"), Qt::CaseInsensitive) == 0;
    };

    QList<QUrl> accepted;
    accepted.reserve(files.size());
    for (const QUrl& u : files) {
        const QFileInfo fi(u.toLocalFile());
        const QString suffixLower = fi.suffix().toLower();
        if (supportedSuffixes.contains(suffixLower) || isPdfPath(fi.absoluteFilePath())) {
            accepted.append(u);
        }
    }

    if (accepted.isEmpty()) {
        qWarning() << "No supported image/PDF files provided to cwSurveyNoteModel::addFromFiles";
        return;
    }

    auto* proj = project();
    if (proj == nullptr) {
        qWarning() << "Project not found for cwSurveyNoteModel::addFromFiles";
        return;
    }

    const QDir destinationDirectory = cwSaveLoad::dir(this);

    proj->addImages(
        accepted,
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
    QList<QObject*> baseNotes;
    baseNotes.reserve(notes.size());
    for(const auto note : notes) {
        baseNotes.append(note);
    }
    cwSurveyNoteModelBase::addNotes(baseNotes);
}

void cwSurveyNoteModel::setData(const cwSurveyNoteModelData &data)
{
    clearNotes();

    QList<QObject*> newNotes;
    newNotes.reserve(data.notes.size());

    for (const auto& noteData : data.notes) {
        auto* note = new cwNote(this);
        note->setParentTrip(parentTrip());
        note->setParentCave(parentCave());
        note->setData(noteData);
        newNotes.append(note);
    }

    cwSurveyNoteModelBase::addNotes(newNotes);
}

cwSurveyNoteModelData cwSurveyNoteModel::data() const
{
    cwSurveyNoteModelData out;
    const auto objNotes = notes();
    out.notes.reserve(objNotes.size());

    for (QObject* obj : objNotes) {
        if (auto* note = qobject_cast<cwNote*>(obj)) {
            out.notes.append(note->data());
        }
    }
    return out;
}

void cwSurveyNoteModel::onParentTripChanged()
{
    const auto notes = this->notes();
    for (QObject* obj : notes) {
        if (auto* note = qobject_cast<cwNote*>(obj)) {
            note->setParentTrip(parentTrip());
            if (parentTrip() != nullptr) {
                note->setParentCave(parentTrip()->parentCave());
            }
        }
    }
}

void cwSurveyNoteModel::onParentCaveChanged()
{
    const auto notes = this->notes();
    for (QObject* obj : notes) {
        if (auto* note = qobject_cast<cwNote*>(obj)) {
            note->setParentCave(parentCave());
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
        note->setName(QFileInfo(image.path()).fileName());
        note->setImage(image);
        note->setParentTrip(parentTrip());
        note->setParentCave(parentCave());
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
