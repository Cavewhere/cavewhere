// LocalSpacingFixtureHelper.h
#pragma once

//Qt includes
#include <QVector>
#include <QVector3D>

//Our includes
#include "cwPointOctree.h"
#include "cwPointOctreeSampler.h"

namespace cwLocalSpacingFixture {

    /**
     * cw::octree::estimateNativeSpacing() as the codes a node's payload
     * carries in each point's reserved word, every node capped at its own
     * grid: what tests that build an octree by hand write beside each point.
     */
    inline QVector<QVector<quint16>> estimateLocalSpacing(const QVector<cw::octree::SampledNode>& nodes,
                                                          const QVector3D& rootMin, double rootSize)
    {
        const QVector<QVector<float>> spacings =
            cw::octree::estimateNativeSpacing(nodes, rootMin, rootSize);
        QVector<QVector<quint16>> codes;
        codes.reserve(nodes.size());
        for(qsizetype i = 0; i < nodes.size(); i++) {
            codes.append(cw::octree::encodeLocalSpacings(
                spacings.at(i),
                cw::octree::levelLocalSpacingCap(rootSize, nodes.at(i).cell.level)));
        }
        return codes;
    }
}
