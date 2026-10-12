/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWPASSAGETUBEMESH_H
#define CWPASSAGETUBEMESH_H

//Qt includes
#include <QString>
#include <QUuid>
#include <QVector>
#include <QVector3D>

/**
 * The triangle mesh cwPassageTubeBuilder produces for a caving region: an
 * indexed triangle list in the line plot's local projection (meters, Z up).
 *
 * Each trip's triangles are contiguous in indices; tripIndexRanges[i] is the
 * span of running trip i and tripUuids[i] its stable cwTripData::id, so
 * keyword visibility can hide one trip's tube by index range.
 */
struct cwPassageTubeMesh {
    // A contiguous span of indices, [first, first + count).
    struct VertexRange {
        int first = 0;
        int count = 0;
    };

    struct Warning {
        enum Type {
            NoPassageData,
            ChainTorn,
            LrudOutOfRange,
            ScrapOpening,
            SourceChange,
            ShaftReading,
            BendClamped,
            ArmInsideThrough,
            JunctionOverlap
        };

        Type type = NoPassageData;
        QUuid tripId;
        QString station;
        QString message;
    };

    QVector<QVector3D> positions;
    QVector<QVector3D> normals;
    QVector<quint32> indices;
    QVector<VertexRange> tripIndexRanges;
    QVector<QUuid> tripUuids;
    QVector<Warning> warnings;

    bool isEmpty() const { return indices.isEmpty(); }
};

#endif // CWPASSAGETUBEMESH_H
