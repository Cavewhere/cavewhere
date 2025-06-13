#pragma once

#include <QByteArray>
#include <QPointF>

#include "cavewhere-sketch.pb.h"   // Generated from your ProtoBuf file
#include "qt.pb.h"             // Generated from qt.proto
#include "PenLineModel.h"      // Your C++ PenLineModel class

// A small helper struct in case you want to return both the parsed model
// and an error flag/message. Not strictly necessary, but can be useful.
struct DeserializationResult {
    bool success = false;
    QString errorString;
    QVector<cwSketch::PenLine> lines;
};

/**
 * @brief The PenLineModelSerializer class
 *
 * Converts between your C++ PenLineModel and the ProtoBuf binary form (`CavewhereSketchProto::PenLineModel`).
 */
class PenLineModelSerializer {
public:
    /**
     * @brief serialize
     *   Takes a const reference to a PenLineModel (your Qt C++ class) and returns
     *   a QByteArray containing the ProtoBuf-serialized bytes.
     *
     * @param model  The in-memory PenLineModel to serialize
     * @return QByteArray  Raw bytes of the ProtoBuf message
     */
    static QByteArray serialize(const cwSketch::PenLineModel& model);

    /**
     * @brief deserialize
     *   Attempts to parse a QByteArray (which is expected to be in ProtoBuf form)
     *   back into a freshly allocated PenLineModel. Ownership of the returned
     *   PenLineModel* lies with the caller (delete it when done).
     *
     * @param data  The raw ProtoBuf bytes
     * @return DeserializationResult  Contains either a valid PenLineModel* or an error
     */
    static DeserializationResult deserialize(const QByteArray& data);

private:
    // Helpers for converting between Qt::QPointF <-> QtProto::QPointF
    static void toProtoPointF(const QPointF& in, QtProto::QPointF* out);
    static QPointF fromProtoPointF(const QtProto::QPointF& in);
};

