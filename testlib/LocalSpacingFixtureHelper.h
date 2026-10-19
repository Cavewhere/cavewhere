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
     * cw::octree::measureNativeSpacing()'s spacings as the codes a node's payload
     * carries in each point's reserved word: what tests that build an octree
     * by hand write beside each point. The estimator applies every cap
     * itself (WP15's data-relative one and the absolute clamp).
     */
    inline QVector<QVector<quint16>> estimateLocalSpacing(const QVector<cw::octree::SampledNode>& nodes,
                                                          const QVector3D& rootMin, double rootSize)
    {
        //One subtree with no halo: the whole cloud is its own column
        const QVector<QVector<float>> spacings =
            cw::octree::measureNativeSpacing({&nodes}, {}, rootMin, rootSize).spacings.constFirst();
        QVector<QVector<quint16>> codes;
        codes.reserve(nodes.size());
        for(const QVector<float>& nodeSpacings : spacings) {
            codes.append(cw::octree::encodeLocalSpacings(nodeSpacings));
        }
        return codes;
    }
}
