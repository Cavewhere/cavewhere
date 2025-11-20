#include "PenLineModelSerializer.h"

#include <google/protobuf/util/json_util.h>

#include <QString>
#include <QDataStream> // Optional, for any debugging

//using namespace CavewhereSketchProto;

//--------------------------------------------------------------------------------
// static void toProtoPointF(const QPointF&, QtProto::QPointF*)
//--------------------------------------------------------------------------------
void PenLineModelSerializer::toProtoPointF(
    const QPointF& in,
    QtProto::QPointF* out)
{
    // Simply copy the x/y from Qt into the generated Proto class
    out->set_x(in.x());
    out->set_y(in.y());
}

//--------------------------------------------------------------------------------
// static QPointF fromProtoPointF(const QtProto::QPointF&)
//--------------------------------------------------------------------------------
QPointF PenLineModelSerializer::fromProtoPointF(const QtProto::QPointF& in)
{
    return QPointF(in.x(), in.y());
}

//--------------------------------------------------------------------------------
// static QByteArray serialize(const PenLineModel&)
//--------------------------------------------------------------------------------
QByteArray PenLineModelSerializer::serialize(const cwSketch::PenLineModel &model)
{
    // 1) Build a ProtoBuf object
    CavewhereSketchProto::PenLineModel protoModel;

    // 2) For each PenLine in model, append a message
    //    Note: PenLineModel::m_lines is private, so we use the public API:
    //          addNewLine(), addPoint(), finishNewLine() only works for building from scratch,
    //          but for serialization we need direct access to the data. We assume
    //          you have a getter or can friend-access m_lines.
    //
    //    If you do NOT have a public getter for m_lines, you could add:
    //      const QVector<PenLine>& lines() const { return m_lines; }
    //
    //    For now, let’s assume you have a “lines()” accessor. If not, mark PenLineModelSerializer
    //    as a friend of PenLineModel so you can read m_lines directly.
    //
    const auto& lines = model.lines(); // <-- hypothetical public getter

    for (const cwSketch::PenLine& line : lines) {
        // Each PenLine in proto holds only repeated PenPoint. (Width is not in the proto as given.)
        CavewhereSketchProto::PenLine* protoLine = protoModel.add_lines();
        for (const cwSketch::PenPoint& point : line.points) {
            CavewhereSketchProto::PenPoint* protoPoint = protoLine->add_points();

            // convert QPointF -> QtProto::QPointF
            toProtoPointF(point.position, protoPoint->mutable_position());

            // copy pressure
            protoPoint->set_pressure(point.pressure);
        }
    }


    // Convert `protoModel` to a JSON string:
    std::string jsonStd;
    auto status = google::protobuf::util::MessageToJsonString(protoModel, &jsonStd);
    // if (!status.ok()) {
    //     qWarning() << "Proto → JSON failed:" << status.ToString();
    //     return {};
    // }

    // qDebug() << "Json:" << jsonStd;

    // // 3) Serialize to a QByteArray
    // int byteCount = protoModel.ByteSizeLong();
    QByteArray buffer = QByteArray::fromStdString(std::move(jsonStd));
    // buffer.resize(byteCount);
    // protoModel.SerializeToArray(buffer.data(), buffer.size());
    // qDebug() << "buffer size:" << buffer.size() << buffer;

    // google::protobuf::util::BinaryToJsonString()

    return buffer;
}

DeserializationResult PenLineModelSerializer::deserialize(const QByteArray& data)
{
    DeserializationResult result;

    CavewhereSketchProto::PenLineModel protoModel;

    // Convert the incoming QByteArray (JSON text) into a std::string:
    std::string jsonStd{ data.constData(), static_cast<size_t>(data.size()) };

    // Attempt to parse JSON → Proto
    auto status = google::protobuf::util::JsonStringToMessage(jsonStd, &protoModel);
    if (!status.ok()) {
        result.success = false;
        result.errorString = QStringLiteral("Failed to parse PenLineModel JSON: %1")
                                 .arg(QString::fromStdString(status.ToString()));
        return result;
    }

    // Prepare a QVector<PenLine> to return
    QVector<cwSketch::PenLine> cppLines;
    cppLines.reserve(protoModel.lines_size());

    // For each ProtoBuf PenLine, build a cwSketch::PenLine
    for (int i = 0; i < protoModel.lines_size(); ++i) {
        const CavewhereSketchProto::PenLine& protoLine = protoModel.lines(i);

        cwSketch::PenLine line;
        // If you added a 'width' field to the .proto, set it here:
        // line.width = protoLine.has_width() ? protoLine.width() : 2.5;

        // Convert each ProtoBuf PenPoint → cwSketch::PenPoint
        for (int j = 0; j < protoLine.points_size(); ++j) {
            const CavewhereSketchProto::PenPoint& protoPoint = protoLine.points(j);

            const QtProto::QPointF& protoQp = protoPoint.position();
            QPointF pos = fromProtoPointF(protoQp);
            double pressure = protoPoint.pressure();

            cwSketch::PenPoint cppPoint(pos, pressure);
            line.points.append(cppPoint);
        }

        cppLines.append(std::move(line));
    }

    result.success = true;
    result.lines = std::move(cppLines);
    return result;
}
