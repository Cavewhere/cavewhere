#pragma once

//Our includes
#include "CaveWhereLibExport.h"

//Data types used in signatures
#include "cwTripCalibration.h"
#include "cwTeam.h"
#include "cwTeamData.h"
#include "cwSurveyChunkData.h"
#include "cwScrapData.h"
#include "cwNoteTransformationData.h"
#include "cwNoteLiDARTransformationData.h"
#include "cwLength.h"
#include "cwImageResolution.h"
#include "cwProjectedProfileScrapViewMatrix.h"
#include "cwStation.h"
#include "cwShot.h"
#include "cwNoteStation.h"
#include "cwLead.h"
#include "cwImage.h"
#include "cwPenPoint.h"
#include "cwPenStroke.h"
#include "cwScale.h"
#include "cwSketchData.h"

//Forward declarations
class cwTripCalibration;
class cwSurveyChunk;
class cwTeam;
class cwScrap;
class cwAbstractNoteTransformation;
class cwNoteLiDARTransformation;

//Google protobuffer
namespace CavewhereProto {
class TripCalibration;
class Team;
class TeamMember;
class SurveyChunk;
class StationShot;
class Scrap;
class NoteStation;
class Lead;
class NoteTransformation;
class NoteLiDARTransformation;
class ProjectedProfileScrapViewMatrix;
class ImageResolution;
class Length;
class Image;
class PenPoint;
class PenStroke;
class Scale;
class Sketch;
}

namespace QtProto {
class QString;
class QDate;
class QSize;
class QSizeF;
class QPointF;
class QVector3D;
class QVector2D;
class QQuaternion;
}

namespace google::protobuf {
template <typename T> class RepeatedPtrField;
}

//Qt includes
#include <QDate>
#include <QSize>
#include <QSizeF>
#include <QPointF>
#include <QVector3D>
#include <QVector2D>
#include <QQuaternion>
#include <QUuid>
#include <QString>
#include <QStringList>

//std includes
#include <memory>
#include <string>

namespace cwProtoUtils {

// UUID helpers
CAVEWHERE_LIB_EXPORT QUuid toUuid(const std::string& uuidStr);

// Legacy QtProto::QString helper
CAVEWHERE_LIB_EXPORT QString fromLegacyQtString(const QtProto::QString& protoString);

// String list helper
CAVEWHERE_LIB_EXPORT QStringList fromProtoStringList(const google::protobuf::RepeatedPtrField<std::string>& protoStringList);

// Qt primitive load helpers
CAVEWHERE_LIB_EXPORT QDate loadDate(const QtProto::QDate& protoDate);
CAVEWHERE_LIB_EXPORT QSize loadSize(const QtProto::QSize& protoSize);
CAVEWHERE_LIB_EXPORT QSizeF loadSizeF(const QtProto::QSizeF& protoSize);
CAVEWHERE_LIB_EXPORT QPointF loadPointF(const QtProto::QPointF& protoPointF);
CAVEWHERE_LIB_EXPORT QVector3D loadVector3D(const QtProto::QVector3D& protoVector3D);
CAVEWHERE_LIB_EXPORT QVector2D loadVector2D(const QtProto::QVector2D& protoVector2D);

// Qt primitive save helpers
CAVEWHERE_LIB_EXPORT void saveString(std::string* protoString, const QString& string);
CAVEWHERE_LIB_EXPORT void saveDate(QtProto::QDate* protoDate, QDate date);
CAVEWHERE_LIB_EXPORT void saveSize(QtProto::QSize* protoSize, QSize size);
CAVEWHERE_LIB_EXPORT void saveSizeF(QtProto::QSizeF* protoSize, QSizeF size);
CAVEWHERE_LIB_EXPORT void savePointF(QtProto::QPointF* protoPointF, QPointF point);
CAVEWHERE_LIB_EXPORT void saveVector3D(QtProto::QVector3D* protoVector3D, QVector3D vector3D);
CAVEWHERE_LIB_EXPORT void saveQUuid(std::string* protoString, const QUuid& id);
CAVEWHERE_LIB_EXPORT void saveStringList(google::protobuf::RepeatedPtrField<std::string>* protoStringList, const QStringList& stringList);

// Complex save helpers
CAVEWHERE_LIB_EXPORT void saveLength(CavewhereProto::Length* protoLength, cwLength* length);
CAVEWHERE_LIB_EXPORT void saveLength(CavewhereProto::Length* protoLength, const cwLength::Data& data);
CAVEWHERE_LIB_EXPORT void saveImageResolution(CavewhereProto::ImageResolution* protoImageRes, cwImageResolution* imageResolution);
CAVEWHERE_LIB_EXPORT void saveImage(CavewhereProto::Image* protoImage, const cwImage& image);
CAVEWHERE_LIB_EXPORT void saveNoteStation(CavewhereProto::NoteStation* protoNoteStation, const cwNoteStation& noteStation);
CAVEWHERE_LIB_EXPORT void saveTeamMember(CavewhereProto::TeamMember* protoTeamMember, const cwTeamMember& teamMember);
CAVEWHERE_LIB_EXPORT void saveLead(CavewhereProto::Lead* protoLead, const cwLead& lead);
CAVEWHERE_LIB_EXPORT void saveProjectedScrapViewMatrix(CavewhereProto::ProjectedProfileScrapViewMatrix* protoViewMatrix,
                                                       cwProjectedProfileScrapViewMatrix* viewMatrix);
CAVEWHERE_LIB_EXPORT void saveNoteTranformation(CavewhereProto::NoteTransformation* protoNoteTransformation,
                                                cwAbstractNoteTransformation* noteTransformation);
CAVEWHERE_LIB_EXPORT void saveStationShot(CavewhereProto::StationShot* protoStation, const cwStation& station);
CAVEWHERE_LIB_EXPORT void saveStationShot(CavewhereProto::StationShot* protoShot, const cwShot& shot);
CAVEWHERE_LIB_EXPORT void saveTripCalibration(CavewhereProto::TripCalibration* protoTripCalibration, cwTripCalibration* tripCalibration);
CAVEWHERE_LIB_EXPORT void saveSurveyChunk(CavewhereProto::SurveyChunk* protoChunk, cwSurveyChunk* chunk);
CAVEWHERE_LIB_EXPORT void saveTeam(CavewhereProto::Team* protoTeam, cwTeam* team);
CAVEWHERE_LIB_EXPORT void saveScrap(CavewhereProto::Scrap* protoScrap, cwScrap* scrap);

// LiDAR save helpers
CAVEWHERE_LIB_EXPORT void saveNoteLiDARTranformation(CavewhereProto::NoteLiDARTransformation* protoNoteTransformation,
                                                     cwNoteLiDARTransformation* noteTransformation);
CAVEWHERE_LIB_EXPORT void saveQQuaternion(QtProto::QQuaternion* protoQuaternion, const QQuaternion& quaternion);

// fromProto* deserialization
CAVEWHERE_LIB_EXPORT cwTripCalibrationData fromProtoTripCalibration(const CavewhereProto::TripCalibration& proto);
CAVEWHERE_LIB_EXPORT cwTeamData fromProtoTeam(const CavewhereProto::Team& proto);
CAVEWHERE_LIB_EXPORT cwTeamMember fromProtoTeamMember(const CavewhereProto::TeamMember& proto);
CAVEWHERE_LIB_EXPORT cwSurveyChunkData fromProtoSurveyChunk(const CavewhereProto::SurveyChunk& protoChunk);
CAVEWHERE_LIB_EXPORT cwStation fromProtoStation(const CavewhereProto::StationShot& protoStation);
CAVEWHERE_LIB_EXPORT cwShot fromProtoShot(const CavewhereProto::StationShot& protoShot);
CAVEWHERE_LIB_EXPORT cwScrapData fromProtoScrap(const CavewhereProto::Scrap& protoScrap);
CAVEWHERE_LIB_EXPORT cwNoteStation fromProtoNoteStation(const CavewhereProto::NoteStation& protoNoteStation);
CAVEWHERE_LIB_EXPORT cwLead fromProtoLead(const CavewhereProto::Lead& protoLead);
CAVEWHERE_LIB_EXPORT cwNoteTransformationData fromProtoNoteTransformation(const CavewhereProto::NoteTransformation& protoNoteTransform);
CAVEWHERE_LIB_EXPORT cwNoteLiDARTransformationData fromProtoLiDARNoteTransformation(const CavewhereProto::NoteLiDARTransformation& protoNoteTransform);
CAVEWHERE_LIB_EXPORT std::unique_ptr<cwProjectedProfileScrapViewMatrix::Data> fromProtoProjectedScraptViewMatrix(const CavewhereProto::ProjectedProfileScrapViewMatrix protoViewMatrix);
CAVEWHERE_LIB_EXPORT cwImageResolution::Data fromProtoImageResolution(const CavewhereProto::ImageResolution& protoImageResolution);
CAVEWHERE_LIB_EXPORT QQuaternion fromProtoQuaternion(const QtProto::QQuaternion& protoQuaternion);
CAVEWHERE_LIB_EXPORT cwLength::Data fromProtoLength(const CavewhereProto::Length& protoLength);

// Sketch
CAVEWHERE_LIB_EXPORT void savePenPoint (CavewhereProto::PenPoint*  protoPoint,  const cwPenPoint&    point);
CAVEWHERE_LIB_EXPORT void savePenStroke(CavewhereProto::PenStroke* protoStroke, const cwPenStroke&   stroke);
CAVEWHERE_LIB_EXPORT void saveScale    (CavewhereProto::Scale*     protoScale,  const cwScale::Data& scale);
CAVEWHERE_LIB_EXPORT void saveSketch   (CavewhereProto::Sketch*    protoSketch, const cwSketchData&  data);

CAVEWHERE_LIB_EXPORT cwPenPoint    fromProtoPenPoint (const CavewhereProto::PenPoint&  protoPoint);
CAVEWHERE_LIB_EXPORT cwPenStroke   fromProtoPenStroke(const CavewhereProto::PenStroke& protoStroke);
CAVEWHERE_LIB_EXPORT cwScale::Data fromProtoScale    (const CavewhereProto::Scale&     protoScale);
CAVEWHERE_LIB_EXPORT cwSketchData  fromProtoSketch   (const CavewhereProto::Sketch&    protoSketch);

} // namespace cwProtoUtils
