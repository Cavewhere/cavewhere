/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our include
#include "cwNote.h"
#include "cwTrip.h"
#include "cwScrap.h"
#include "cwDebug.h"
#include "cwImageResolution.h"
#include "cwSurveyNoteModel.h"

//Std includes
#include "cwMath.h"

//Qt includes
#include <QDebug>

cwNote::cwNote(QObject *parent) :
    QObject(parent),
    ParentTrip(nullptr),
    ParentCave(nullptr),
    ImageResolution(new cwImageResolution(this))
{
    DisplayRotation = 0.0;
    ImageResolution->setUpdateValue(true); //Update the value when the units have changed

    connect(ImageResolution, &cwImageResolution::unitChanged, this, &cwNote::updateScrapNoteTransform);
    connect(ImageResolution, &cwImageResolution::valueChanged, this, &cwNote::updateScrapNoteTransform);
}


// cwNote::cwNote(const cwNote& object) :
//     QObject(nullptr),
//     ParentTrip(nullptr),
//     ParentCave(nullptr),
//     ImageResolution(new cwImageResolution(this))
// {
//     copy(object);

//     connect(ImageResolution, &cwImageResolution::unitChanged, this, &cwNote::updateScrapNoteTransform);
//     connect(ImageResolution, &cwImageResolution::valueChanged, this, &cwNote::updateScrapNoteTransform);
// }

// cwNote& cwNote::operator=(const cwNote& object) {
//     if(&object == this) { return *this; }
//     copy(object);
//     return *this;
// }

// /**
//   Does a deap copy of the note
//   */
// void cwNote::copy(const cwNote& object) {
//     ImageIds = object.ImageIds;
//     DisplayRotation = object.DisplayRotation;

//     //Delete extra scraps
//     for(int i = Scraps.size() - 1; i >= object.scraps().size(); i--) {
//         Scraps.at(i)->deleteLater();
//         Scraps.removeLast();
//     }

//     Q_ASSERT(Scraps.size() <= object.scraps().size());

//     //Update already created scraps
//     for(int i = 0; i < Scraps.size(); i++) {
//         cwScrap* oldScrap = object.scraps().at(i);
//         cwScrap* newScrap = object.scraps().at(i);
//         *oldScrap = *newScrap;
//     }

//     //Add new scraps if needed
//     for(int i = Scraps.size(); i < object.scraps().size(); i++) {
//         cwScrap* otherScrap = object.scrap(i);
//         cwScrap* scrap = new cwScrap(*otherScrap);
//         setupScrap(scrap);
//         Scraps.append(scrap);
//     }

//     //Copy the image resolution
//     *ImageResolution = *(object.ImageResolution);

//     Q_ASSERT(Scraps.size() == object.scraps().size());
// }

/**
  \brief Sets the image data for the page of notes

  \param cwImage - This is the object that stores the ids to all the image
  data on disk
  */
void cwNote::setImage(cwImage image) {
    if(ImageIds != image) {
        ImageIds = image;
        // emit originalChanged(ImageIds.original());
        // emit iconChanged(ImageIds.icon());
        emit imageChanged();

        resetImageResolution();
    }
}

/**
  \brief Sets the rotation for the image

  \param degrees - The rotation in degrees
  */
void cwNote::setRotate(double degrees) {
    degrees = fmod(degrees, 360.0);
    if(degrees != DisplayRotation) {
        DisplayRotation = degrees;
        emit rotateChanged(DisplayRotation);
    }
}

/**
  \brief Sets the parent trip for this note
  */
void cwNote::setParentTrip(cwTrip* trip) {
    if(ParentTrip != trip) {
        ParentTrip = trip;
        setParent(trip);
        setParentCave(trip->parentCave());
    }
}

/**
  \brief Gets the scaling matrix for the orignial size of the notes
  */
QMatrix4x4 cwNote::scaleMatrix() const {
    QSize size = ImageIds.originalSize();
    QMatrix4x4 matrix;
    matrix.scale(size.width(), size.height(), 1.0);
    return matrix;
}

/**
  \brief Returns a matrix that can convert the normalize note coordinates to the physical
  page distance in meters.  This is useful for converting normalized coordinates into
  reallife coordinates.
  */
QMatrix4x4 cwNote::metersOnPageMatrix() const {
    double dotsPerMeter = dotPerMeter();
    double metersPerDot = 1.0 / dotsPerMeter;

    QMatrix4x4 metersPerDotsMatrix;
    metersPerDotsMatrix.scale(metersPerDot, metersPerDot, 1.0);

    return metersPerDotsMatrix * scaleMatrix();
}


/**
  \Brief adds a scrap to the notes
  */
void cwNote::addScrap(cwScrap* scrap) {
    if(scrap == nullptr) {
        qDebug() << "This is a bug! scrap is null and shouldn't be" << LOCATION;
        return;
    }

    int lastIndex = Scraps.size();

    emit beginInsertingScraps(lastIndex, lastIndex);

    setupScrap(scrap);
    Scraps.append(scrap);


    emit insertedScraps(lastIndex, lastIndex);
}

/**
  \brief Gets all the scraps from the notes
  */
QList<cwScrap*> cwNote::scraps() const {
    return Scraps;
}

/**
  Sets the scraps for the note
  */
void cwNote::setScraps(QList<cwScrap *> scraps) {

    Scraps = scraps;

    //Go through all the scraps and set the parents
    foreach(cwScrap* scrap, Scraps) {
        setupScrap(scrap);
    }

    emit scrapsReset();
}

/**
 * @brief cwNote::removeScraps
 * @param begin
 * @param end
 *
 * Removes and deletes all the scraps between begin and end index
 */
void cwNote::removeScraps(int begin, int end)
{
    emit beginRemovingScraps(begin, end);

    for(int i = end; i >= begin; i--) {
        Scraps.at(i)->deleteLater();
        Scraps.removeAt(i);
    }

    emit removedScraps(begin, end);
}



/**
  Gets the scrap form the note at a index

  If the scrapIndex is out of bounds, this returns nullptr
  */
cwScrap* cwNote::scrap(int scrapIndex) const {
    if(scrapIndex >= 0 && scrapIndex < Scraps.size()) {
        return Scraps[scrapIndex];
    }
    return nullptr;
}

/**
  \brief Sets up the scraps data
  */
void cwNote::setupScrap(cwScrap *scrap) {
    scrap->setParent(this);
    scrap->setParentNote(this);
    scrap->setParentCave(ParentCave);
}

/**
 * @brief cwNote::updateScrapNoteTransform
 *
 * Goes through all the scraps in the note and updates the scrap transform
 */
void cwNote::updateScrapNoteTransform()
{
    foreach(cwScrap* scrap, scraps()) {
        scrap->updateNoteTransformation();
    }
}

/**
  \brief Sets the parent cave
  */
void cwNote::setParentCave(cwCave *cave) {
    if(ParentCave != cave) {
        ParentCave = cave;
        foreach(cwScrap* scrap, Scraps) {
            scrap->setParentCave(ParentCave);
        }
    }
}

/**
 * @brief cwNote::dotPerMeter
 * @return Get's the resolution of the image
 */
double cwNote::dotPerMeter() const {
    return imageResolution()->convertTo(cwUnits::DotsPerMeter).value;
}

/**
 * @brief cwNote::resetImageResolution
 *
 * This resets the image resolution to it's original resolution
 */
void cwNote::resetImageResolution() {
    imageResolution()->setUpdateValue(false);
    imageResolution()->setUnit(cwUnits::DotsPerMeter);
    imageResolution()->setValue(image().originalDotsPerMeter());
    imageResolution()->setUpdateValue(true);
    imageResolution()->setUnit(cwUnits::DotsPerInch); //Automatically converts imageResolution to inch
}

/**
 * @brief cwNote::propagateResolutionNotesInTrip
 *
 * This uses the parent trip, if valid, and propagates the current resolution to all the other
 * notes
 */
void cwNote::propagateResolutionNotesInTrip()
{
    if(parentTrip() == nullptr) { return; }

    foreach(cwNote* note, parentTrip()->notes()->notes()) {
        note->imageResolution()->setData(ImageResolution->data());
    }
}

cwNoteData cwNote::data() const
{
    QList<cwScrapData> scrapData;
    scrapData.reserve(Scraps.size());
    for(auto scrap : Scraps) {
        scrapData.append(scrap->data());
    }

    return {
        m_name,
        DisplayRotation,
        ImageResolution->data(),
        std::move(scrapData),
        ImageIds
    };
}

void cwNote::setData(const cwNoteData &data)
{
    m_name = data.name;
    setRotate(data.rotate);
    ImageResolution->setData(data.imageResolution);

    //Old image data, for loading from old files
    setImage(data.image);

    //Delete extra scraps
    for(int i = Scraps.size() - 1; i >= data.scraps.size(); i--) {
        Scraps.at(i)->deleteLater();
        Scraps.removeLast();
    }

    Q_ASSERT(Scraps.size() <= data.scraps.size());

    //Update already created scraps
    for(int i = 0; i < Scraps.size(); i++) {
        Scraps.at(i)->setData(data.scraps.at(i));
    }

    //Add new scraps if needed
    for(int i = Scraps.size(); i < data.scraps.size(); i++) {
        cwScrap* scrap = new cwScrap();
        setupScrap(scrap);
        scrap->setData(data.scraps.at(i));
        Scraps.append(scrap);
    }

    emit scrapsReset();
}
