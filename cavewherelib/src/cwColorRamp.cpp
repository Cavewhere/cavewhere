// Our includes
#include "cwColorRamp.h"

// Std includes
#include <algorithm>
#include <cmath>

namespace {

// sRGB transfer function (IEC 61966-2-1)
constexpr double kSrgbLinearThreshold = 0.04045;
constexpr double kLinearSrgbThreshold = 0.0031308;
constexpr double kSrgbLinearSlope = 12.92;
constexpr double kSrgbOffset = 0.055;
constexpr double kSrgbScale = 1.055;
constexpr double kSrgbGamma = 2.4;

double srgbToLinear(double c)
{
    if (c <= kSrgbLinearThreshold) {
        return c / kSrgbLinearSlope;
    }
    return std::pow((c + kSrgbOffset) / kSrgbScale, kSrgbGamma);
}

double linearToSrgb(double c)
{
    c = std::clamp(c, 0.0, 1.0);
    if (c <= kLinearSrgbThreshold) {
        return kSrgbLinearSlope * c;
    }
    return kSrgbScale * std::pow(c, 1.0 / kSrgbGamma) - kSrgbOffset;
}

double lerp(double a, double b, double u)
{
    return a + (b - a) * u;
}

// Preset stop colors
constexpr const char* kDepthDeep = "#1e1b4b";
constexpr const char* kDepthBlue = "#2563eb";
constexpr const char* kWhite = "#ffffff";
constexpr const char* kDepthTan = "#d9a066";
constexpr const char* kDepthBrown = "#7c3a0b";

constexpr const char* kBlueLight = "#dbeafe";

constexpr const char* kWarmDark = "#451a03";
constexpr const char* kWarmMid = "#f59e0b";
constexpr const char* kWarmLight = "#fef3c7";

constexpr const char* kViridis[] = {
    "#440154", "#482878", "#3e4989", "#31688e", "#26828e",
    "#1f9e89", "#35b779", "#6ece58", "#fde725"
};

constexpr const char* kCividis[] = {
    "#00224e", "#123570", "#3b496c", "#575d6d", "#707173",
    "#8a8678", "#a59c74", "#c3b369", "#fee838"
};

constexpr const char* kRainbow[] = {
    "#0000ff", "#00ffff", "#00ff00", "#ffff00", "#ff0000"
};

// Okabe-Ito, the colorblind-safe categorical set, in its published order
constexpr const char* kOkabeIto[] = {
    "#E69F00", "#56B4E9", "#009E73", "#F0E442",
    "#0072B2", "#D55E00", "#CC79A7", "#999999"
};

// Loop error: dark green, light green, orange, dark red
constexpr const char* kLoopError[] = {
    "#1a9850", "#a6d96a", "#fdae61", "#a50026"
};

constexpr double kQuarter = 0.25;
constexpr double kHalf = 0.5;
constexpr double kThreeQuarters = 0.75;

// Stops at i / (count - 1), for presets whose stops are evenly spaced
template<size_t N>
QVector<cwColorRamp::Stop> evenStops(const char* const (&colors)[N])
{
    QVector<cwColorRamp::Stop> stops;
    stops.reserve(static_cast<int>(N));
    for (size_t i = 0; i < N; ++i) {
        stops.append({static_cast<double>(i) / static_cast<double>(N - 1), QColor(colors[i])});
    }
    return stops;
}

} // namespace

cwColorRamp::cwColorRamp(const QString& id, const QString& name, Kind kind, const QVector<Stop>& stops) :
    m_id(id),
    m_name(name),
    m_kind(kind),
    m_stops(stops)
{
}

QColor cwColorRamp::colorAt(double t) const
{
    if (m_stops.isEmpty()) {
        return QColor();
    }

    t = std::clamp(t, 0.0, 1.0);

    if (m_kind == Kind::Categorical) {
        const int count = m_stops.size();
        const int slot = std::min(static_cast<int>(std::floor(t * count)), count - 1);
        return m_stops.at(slot).color;
    }

    if (t <= m_stops.first().position) {
        return m_stops.first().color;
    }
    if (t >= m_stops.last().position) {
        return m_stops.last().color;
    }

    int lower = 0;
    for (int i = 0; i < m_stops.size(); ++i) {
        if (m_stops.at(i).position <= t) {
            lower = i;
        }
    }
    const int upper = std::min(lower + 1, static_cast<int>(m_stops.size()) - 1);

    const Stop& a = m_stops.at(lower);
    const Stop& b = m_stops.at(upper);
    const double width = b.position - a.position;
    if (width <= 0.0) {
        return a.color;
    }

    const double u = (t - a.position) / width;
    const Lab labA = toOklab(a.color);
    const Lab labB = toOklab(b.color);
    return fromOklab({
        lerp(labA.l, labB.l, u),
        lerp(labA.a, labB.a, u),
        lerp(labA.b, labB.b, u)
    });
}

QVector<QColor> cwColorRamp::bake(int count) const
{
    QVector<QColor> samples;
    if (count <= 0 || m_stops.isEmpty()) {
        return samples;
    }
    samples.reserve(count);

    if (m_kind == Kind::Categorical) {
        for (int i = 0; i < count; ++i) {
            samples.append(m_stops.at(i % m_stops.size()).color);
        }
        return samples;
    }

    if (count == 1) {
        samples.append(colorAt(0.0));
        return samples;
    }

    for (int i = 0; i < count; ++i) {
        samples.append(colorAt(static_cast<double>(i) / static_cast<double>(count - 1)));
    }
    return samples;
}

QVector<cwColorRamp> cwColorRamp::presets()
{
    static const QVector<cwColorRamp> ramps = {
        cwColorRamp(defaultDepthRampId(), QStringLiteral("Depth"), Kind::Diverging, {
            {0.0, QColor(kDepthDeep)},
            {kQuarter, QColor(kDepthBlue)},
            {kHalf, QColor(kWhite)},
            {kThreeQuarters, QColor(kDepthTan)},
            {1.0, QColor(kDepthBrown)}
        }),
        cwColorRamp(QStringLiteral("blue"), QStringLiteral("Blue"), Kind::Sequential, {
            {0.0, QColor(kDepthDeep)},
            {kHalf, QColor(kDepthBlue)},
            {1.0, QColor(kBlueLight)}
        }),
        cwColorRamp(QStringLiteral("warm"), QStringLiteral("Warm"), Kind::Sequential, {
            {0.0, QColor(kWarmDark)},
            {kHalf, QColor(kWarmMid)},
            {1.0, QColor(kWarmLight)}
        }),
        cwColorRamp(QStringLiteral("viridis"), QStringLiteral("Viridis"), Kind::Sequential, evenStops(kViridis)),
        cwColorRamp(QStringLiteral("cividis"), QStringLiteral("Cividis"), Kind::Sequential, evenStops(kCividis)),
        cwColorRamp(QStringLiteral("rainbow"), QStringLiteral("Rainbow (classic)"), Kind::Sequential, evenStops(kRainbow)),
        cwColorRamp(defaultCategoricalRampId(), QStringLiteral("Okabe–Ito"), Kind::Categorical, evenStops(kOkabeIto))
    };
    return ramps;
}

cwColorRamp cwColorRamp::preset(const QString& id)
{
    const QVector<cwColorRamp> ramps = presets();
    for (const cwColorRamp& ramp : ramps) {
        if (ramp.id() == id) {
            return ramp;
        }
    }
    return cwColorRamp();
}

QString cwColorRamp::defaultDepthRampId()
{
    return QStringLiteral("depth");
}

cwColorRamp cwColorRamp::loopErrorRamp()
{
    static const cwColorRamp ramp(QStringLiteral("loop-error"), QStringLiteral("Loop error"), Kind::Sequential,
                                  evenStops(kLoopError));
    return ramp;
}

QString cwColorRamp::defaultCategoricalRampId()
{
    return QStringLiteral("okabe-ito");
}

// OKLab conversion, Bjorn Ottosson's published matrices
// (https://bottosson.github.io/posts/oklab/)
cwColorRamp::Lab cwColorRamp::toOklab(const QColor& color)
{
    const double r = srgbToLinear(color.redF());
    const double g = srgbToLinear(color.greenF());
    const double b = srgbToLinear(color.blueF());

    const double l = std::cbrt(0.4122214708 * r + 0.5363325363 * g + 0.0514459929 * b);
    const double m = std::cbrt(0.2119034982 * r + 0.6806995451 * g + 0.1073969566 * b);
    const double s = std::cbrt(0.0883024619 * r + 0.2817188376 * g + 0.6299787005 * b);

    return {
        0.2104542553 * l + 0.7936177850 * m - 0.0040720468 * s,
        1.9779984951 * l - 2.4285922050 * m + 0.4505937099 * s,
        0.0259040371 * l + 0.7827717662 * m - 0.8086757660 * s
    };
}

QColor cwColorRamp::fromOklab(const Lab& lab)
{
    const double l_ = lab.l + 0.3963377774 * lab.a + 0.2158037573 * lab.b;
    const double m_ = lab.l - 0.1055613458 * lab.a - 0.0638541728 * lab.b;
    const double s_ = lab.l - 0.0894841775 * lab.a - 1.2914855480 * lab.b;

    const double l = l_ * l_ * l_;
    const double m = m_ * m_ * m_;
    const double s = s_ * s_ * s_;

    const double r = +4.0767416621 * l - 3.3077115913 * m + 0.2309699292 * s;
    const double g = -1.2684380046 * l + 2.6097574011 * m - 0.3413193965 * s;
    const double b = -0.0041960863 * l - 0.7034186147 * m + 1.7076147010 * s;

    return QColor::fromRgbF(static_cast<float>(linearToSrgb(r)),
                            static_cast<float>(linearToSrgb(g)),
                            static_cast<float>(linearToSrgb(b)));
}
