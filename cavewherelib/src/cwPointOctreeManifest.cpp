// cwPointOctreeManifest.cpp
#include "cwPointOctreeManifest.h"
#include "cavewhere.pb.h"

//Std includes
#include <cmath>

namespace {
    //A QVector3D travels as three repeated doubles
    constexpr int kVectorSize = 3;

    QVector3D toVector3D(const google::protobuf::RepeatedField<double>& field)
    {
        return QVector3D(static_cast<float>(field.Get(0)),
                         static_cast<float>(field.Get(1)),
                         static_cast<float>(field.Get(2)));
    }

    void addVector3D(google::protobuf::RepeatedField<double>* field, const QVector3D& vector)
    {
        field->Add(vector.x());
        field->Add(vector.y());
        field->Add(vector.z());
    }
}

QBox3D cwPointOctreeManifest::nodeBounds(int index) const
{
    if(index < 0 || index >= nodes.size()) {
        return QBox3D();
    }

    const cwPointOctreeNode& node = nodes.at(index);
    const double cellSize = std::ldexp(rootSize, -node.level);

    const QVector3D minimum(static_cast<float>(rootMin.x() + node.x * cellSize),
                            static_cast<float>(rootMin.y() + node.y * cellSize),
                            static_cast<float>(rootMin.z() + node.z * cellSize));
    const float edge = static_cast<float>(cellSize);

    return QBox3D(minimum, minimum + QVector3D(edge, edge, edge));
}

double cwPointOctreeManifest::spacing(int level) const
{
    return std::ldexp(rootSize / cw::octree::kSampleGridResolution, -level);
}

const QVector<int>& cwPointOctreeManifest::parents() const
{
    if(m_parents.size() != nodes.size()) {
        m_parents = QVector<int>(nodes.size(), -1);
        for(int i = 0; i < nodes.size(); i++) {
            for(int child : nodes.at(i).children) {
                if(child >= 0 && child < nodes.size()) {
                    m_parents[child] = i;
                }
            }
        }
    }
    return m_parents;
}

QString cwPointOctreeManifest::nodeName(int index) const
{
    if(index < 0 || index >= nodes.size()) {
        return QString();
    }

    const QVector<int>& parentTable = parents();

    QVector<int> octantPath;
    int current = index;
    //The walk is bounded by the node count, so a hand-built cycle still terminates
    for(int step = 0; step < nodes.size() && parentTable.at(current) >= 0; step++) {
        const int parent = parentTable.at(current);
        const std::array<int, cw::octree::kChildCount>& children = nodes.at(parent).children;
        for(int octant = 0; octant < cw::octree::kChildCount; octant++) {
            if(children.at(octant) == current) {
                octantPath.prepend(octant);
                break;
            }
        }
        current = parent;
    }

    return cw::octree::nodeName(octantPath);
}

bool cwPointOctreeManifest::isValid() const
{
    if(nodes.isEmpty()) {
        return false;
    }

    if(nodes.at(0).level != 0) {
        return false;
    }

    for(const cwPointOctreeNode& node : nodes) {
        for(int child : node.children) {
            if(child == -1) {
                continue;
            }

            if(child < 0 || child >= nodes.size()) {
                return false;
            }

            if(nodes.at(child).level != node.level + 1) {
                return false;
            }
        }
    }

    return true;
}

QByteArray cwPointOctreeManifest::serialize() const
{
    CavewhereProto::PointOctreeManifest proto;
    proto.set_format_generation(cw::octree::kFormatGeneration);
    proto.set_fingerprint(fingerprint.toStdString());
    proto.set_point_count(pointCount);
    proto.set_root_size(rootSize);
    proto.set_mean_spacing_xy(meanSpacingXY);

    addVector3D(proto.mutable_root_min(), rootMin);
    addVector3D(proto.mutable_bbox_min(), bboxMin);
    addVector3D(proto.mutable_bbox_max(), bboxMax);

    for(const cwPointOctreeNode& node : nodes) {
        CavewhereProto::PointOctreeNode* nodeProto = proto.add_nodes();
        nodeProto->set_level(static_cast<quint32>(node.level));
        nodeProto->set_x(node.x);
        nodeProto->set_y(node.y);
        nodeProto->set_z(node.z);
        nodeProto->set_point_count(node.pointCount);
        nodeProto->set_byte_size(node.byteSize);
        for(int child : node.children) {
            nodeProto->add_children(child);
        }
    }

    const std::string bytes = proto.SerializeAsString();
    return QByteArray(bytes.data(), static_cast<qsizetype>(bytes.size()));
}

std::optional<cwPointOctreeManifest> cwPointOctreeManifest::deserialize(const QByteArray& data)
{
    CavewhereProto::PointOctreeManifest proto;
    if(!proto.ParseFromArray(data.constData(), static_cast<int>(data.size()))) {
        return std::nullopt;
    }

    if(proto.format_generation() != cw::octree::kFormatGeneration) {
        return std::nullopt;
    }

    if(proto.root_min_size() != kVectorSize
       || proto.bbox_min_size() != kVectorSize
       || proto.bbox_max_size() != kVectorSize) {
        return std::nullopt;
    }

    cwPointOctreeManifest manifest;
    manifest.fingerprint = QString::fromStdString(proto.fingerprint());
    manifest.pointCount = proto.point_count();
    manifest.rootSize = proto.root_size();
    manifest.meanSpacingXY = proto.mean_spacing_xy();
    manifest.rootMin = toVector3D(proto.root_min());
    manifest.bboxMin = toVector3D(proto.bbox_min());
    manifest.bboxMax = toVector3D(proto.bbox_max());

    manifest.nodes.reserve(proto.nodes_size());
    for(const CavewhereProto::PointOctreeNode& nodeProto : proto.nodes()) {
        if(nodeProto.children_size() != cw::octree::kChildCount) {
            return std::nullopt;
        }

        if(nodeProto.level() > static_cast<quint32>(cw::octree::kMaxLevel)) {
            return std::nullopt;
        }

        cwPointOctreeNode node;
        node.level = static_cast<int>(nodeProto.level());
        node.x = nodeProto.x();
        node.y = nodeProto.y();
        node.z = nodeProto.z();
        node.pointCount = nodeProto.point_count();
        node.byteSize = nodeProto.byte_size();
        for(int octant = 0; octant < cw::octree::kChildCount; octant++) {
            node.children[octant] = nodeProto.children(octant);
        }

        manifest.nodes.append(node);
    }

    if(!manifest.isValid()) {
        return std::nullopt;
    }

    return manifest;
}
