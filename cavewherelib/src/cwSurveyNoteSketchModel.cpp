#include "cwSurveyNoteSketchModel.h"

// CaveWhere
#include "cwSketch.h"
#include "cwTrip.h"

// Qt
#include <QUrl>

cwSurveyNoteSketchModel::cwSurveyNoteSketchModel(QObject* parent)
    : cwSurveyNoteModelBase(parent)
{
}

void cwSurveyNoteSketchModel::addFromFiles(QList<QUrl> files)
{
    Q_UNUSED(files);
    // Sketches are not imported from files; the concat model's addSketch()
    // path is the canonical entry point.
}

QVariant cwSurveyNoteSketchModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    if (role == NoteObjectRole) {
        return cwSurveyNoteModelBase::data(index, role);
    }

    const auto* sketch = qobject_cast<cwSketch*>(notes().value(index.row()));
    if (sketch == nullptr) {
        return QVariant();
    }

    switch (role) {
    case PathRole:
        return QVariant();
    case IconPathRole:
        return sketch->iconImagePath();
    case ImageRole:
        return QVariant();
    default:
        return QVariant();
    }
}

cwSketch* cwSurveyNoteSketchModel::addSketch(cwSketch::ViewType viewType)
{
    auto* sketch = new cwSketch();
    sketch->setViewType(viewType);

    // Seed the new sketch's paper scale units from the project default, the same
    // way cwCave seeds a new trip's survey unit. cwTrip::unitSystem() resolves the
    // region, falling back to Metric when the trip has no cave/region yet. Loaded
    // sketches keep their stored scale (setData), so only new sketches are seeded.
    const cwTrip* trip = parentTrip();
    sketch->seedDefaultScale(trip ? trip->unitSystem() : cwUnits::Metric);

    addSketches({ sketch });
    return sketch;
}

void cwSurveyNoteSketchModel::addSketches(const QList<cwSketch*>& sketches)
{
    for (cwSketch* sketch : sketches) {
        if (sketch == nullptr) {
            continue;
        }
        connect(sketch, &cwSketch::iconImagePathChanged, this, [this, sketch]() {
            const int row = notes().indexOf(static_cast<QObject*>(sketch));
            const QModelIndex modelIndex = index(row);
            if (modelIndex.isValid()) {
                emit dataChanged(modelIndex, modelIndex, { IconPathRole });
            }
        });
    }
    addNotesHelper(sketches);
}

void cwSurveyNoteSketchModel::setData(const cwSurveyNoteSketchModelData& data)
{
    setDataHelper<cwSurveyNoteSketchModelData, cwSketch>(data);
}

cwSurveyNoteSketchModelData cwSurveyNoteSketchModel::data() const
{
    return dataHelper<cwSurveyNoteSketchModelData, cwSketch>();
}

void cwSurveyNoteSketchModel::onParentTripChanged()
{
    const auto notes = this->notes();
    for (QObject* obj : notes) {
        if (auto* sketch = qobject_cast<cwSketch*>(obj)) {
            sketch->setParentTrip(parentTrip());
        }
    }
}
