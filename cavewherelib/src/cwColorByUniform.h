#ifndef CWCOLORBYUNIFORM_H
#define CWCOLORBYUNIFORM_H

// Qt includes
#include <QByteArray>

// Std includes
#include <cstring>

// std140 layout shared by LinePlot.frag (ColorByBlock) and scrap.frag
// (the shared middle of ScrapBlock). Keep the GLSL declarations in sync.
struct cwColorByUniform
{
    static constexpr int kRampStops = 32;

    enum Mode : int { Off = 0, Sequential = 1, Diverging = 2, Categorical = 3 };
    enum ValueSource : int { WorldZ = 0, PerElement = 1 };

    float ramp[kRampStops][4] = {};
    float range[4] = {};   // rangeMin, rangeMax, pivot, halfExtent
    float params[4] = {};  // mode, strength, slotCount, valueSource

    static QByteArray toBytes(const cwColorByUniform& uniform)
    {
        return QByteArray(reinterpret_cast<const char*>(&uniform), sizeof(cwColorByUniform));
    }

    bool operator==(const cwColorByUniform& other) const
    {
        return std::memcmp(this, &other, sizeof(cwColorByUniform)) == 0;
    }
    bool operator!=(const cwColorByUniform& other) const { return !(*this == other); }
};

static_assert(sizeof(cwColorByUniform) == 544, "ColorByBlock layout changed; update the shaders");

#endif // CWCOLORBYUNIFORM_H
