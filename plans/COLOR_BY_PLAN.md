# Color By: implementation plan

Color the 3D view by depth (first), then by keyword (cave, trip, year, caver), then
by date, loop membership and loop error. Background and rationale are in
`plans/COLOR_BY_RESEARCH.html`. This file is the authority for what to build.

Status: **plan only, nothing implemented.** Branch `colorBy`.

## 0. Rules for the implementer

Read this section before touching code.

- **One checkpoint at a time.** Build, run the listed tests, write the PNG, then
  **stop and report**. Never start the next checkpoint in the same run. In workflow
  mode the fix-and-commit stage commits each green checkpoint (user, 2026-10-09:
  "The workflow should commit. In workflow mode, it's okay to commit."); other
  stages never commit.
- Build directory: run `ls build/` and use the `Qt_6_11_2_for_macOS_Release` (or
  `_Debug`) directory. Call it `$B` below. Re-run CMake configure after adding a
  file (any build does it; the source globs are `CONFIGURE_DEPENDS`).
  ```bash
  cmake --build $B --target cavewhere-test cavewhere-qml-test CaveWhere
  $B/cavewhere-test "[ColorBy]" 2>&1 | tee /tmp/colorby-test.log
  $B/cavewhere-qml-test --platform offscreen -input test-qml/tst_X.qml 2>&1 | tee /tmp/colorby-qml.log
  ```
  Run only the targeted tests named in the checkpoint. Do not run the full suites.
- Render-to-PNG tests need a GPU window: run those **without** `--platform
  offscreen`, and accept a `skip` on headless CI. Copy the PNG the test wrote to
  `$B/colorby_cp<N>.png` and name that path in the report so the user can look.
- Follow `CLAUDE.md`: 4-space C++ with braces on the next line for functions and on
  the same line for control flow, `m_` members, `k` constants in an anonymous
  namespace, no magic numbers, `QStringLiteral`, `.at(i)` for reads, no bindable
  properties, no `goto`, no test-only methods on production classes. QML: `QC.Label`
  for text, `Theme` tokens for every color and font size, `import QtQuick as QQ`,
  `TapHandler` not `MouseArea`, body ordering id → objectName → properties →
  signals → functions → bindings → handlers → children.
- New C++ files under `cavewherelib/src/` and QML under `cavewherelib/qml/` are
  picked up by globs. New tests under `testcases/*.cpp` and `test-qml/*.qml` too.
- When a snippet below disagrees with the real file, the real file wins for
  surrounding code, the snippet wins for intent. Read the file first.

## 1. The design in one page

### 1.1 One GPU mechanism for every source

Every participating renderer receives the same 544-byte uniform block
(`cwColorByUniform`): a 32-stop RGBA ramp, a range, and a few parameters. The
fragment shader computes

```
t = normalize(value)          // 0..1, rule depends on mode (below)
color = rampColor(t)          // linear blend between the two nearest stops
```

The *value* is either the fragment's world z (depth) or a per-element float
(keyword slot, date ordinal, loop error) chosen by `valueSource`. Nothing else
differs between sources, so the legend, the UBO plumbing and the tests are shared.

### 1.2 The normalize rule, including the positive side

Depth runs from above the datum to far below it, e.g. +100 m to −500 m. **The
pivot (0 by default) is always white**, and the color steps are the same size in
meters on both sides. That is a diverging ramp anchored at the pivot, not at the
middle of the range:

```
mode Sequential:  t = (v - rangeMin) / (rangeMax - rangeMin)
mode Diverging:   halfExtent = max(rangeMax - pivot, pivot - rangeMin)
                  t = 0.5 + 0.5 * (v - pivot) / halfExtent
mode Categorical: slot = v (already an integer slot index); color = ramp[slot]
always:           t is clamped to [0, 1]
```

With +100/−500 and pivot 0: −500 → t = 0 (deepest color), 0 → t = 0.5 (white),
+100 → t = 0.6 (a light tint of the warm side). A cave whose elevations are all
above the pivot (georeferenced, say 1000–1500 m) uses only the warm half, which
still reads as a light→dark sequential ramp. The pivot is user-editable.

### 1.3 Palette rules

- Diverging ramps have white at stop 0.5. Sequential ramps go dark → light with
  t, so the deepest value is darkest. Rainbow exists only as an opt-in preset.
- Stops are interpolated in **OKLab**, which is what keeps a two-stop gradient
  even instead of muddy in the middle.
- Categorical uses Okabe–Ito (8 colors, colorblind safe) in fixed order, slot =
  position of the value in the sorted set of all values in the *unfiltered*
  keyword model, so hiding trips never repaints the remaining ones.

### 1.4 Elements

| Element | Value | How the color lands |
|---|---|---|
| Centerline (`cwRHILinePlot`) | world z, or a per-instance float (CP5) | `ColorByBlock` UBO at binding 1, fragment stage. Splays are lightened toward white. |
| Scraps (`cwRhiTexturedItems`) | world z from the vertex, or a per-item float (CP5) | Per-draw UBO at binding 2 = modelMatrix + shared color-by block + per-item block. Luminance recolor: paper takes the color, ink stays dark. |

### 1.5 UI

- Everything lives in the 3D view, bottom-left, opposite the scale bar and
  compass. A **chip** reading "Color by: None" sits there when color-by is off;
  it becomes the **colorbar legend** (min / pivot / max labels, swatch list for
  categorical) when on. The Layers tab is untouched.
- A single click or right-click on the chip or legend opens a **popup** with the
  controls: source, ramp, pivot, scrap tint. One place, one affordance.
- Persisted in `QSettings` as an app-level view preference.

## 2. Checkpoints

| # | Deliverable | Tests |
|---|---|---|
| 1 | `cwColorRamp`, presets, `cwColorByUniform`, `cwColorBy` state object on the scene manager | `test_cwColorRamp.cpp`, `test_cwColorBy.cpp` |
| 2 | Centerline colored by depth on the GPU | `tst_OffscreenRenderColorBy.qml` |
| 3 | Chip + legend in the view, click-to-open popup with the controls | `tst_ColorByOptions.qml` |
| 4 | Scrap luminance tint | extend `tst_OffscreenRenderColorBy.qml` |
| 5 | Categorical by keyword (cave, trip, year, caver, type) | `test_cwKeywordColorBy.cpp`, `test_cwRenderLinePlotColorValues.cpp` |
| 6 | Date timeline, loop membership, loop error, point cloud | outlined only; plan each separately |

---

## Checkpoint 1: the color library and the state object

No rendering changes. After this checkpoint the app behaves exactly as before.

### 1a. `cavewherelib/src/cwColorRamp.h/.cpp`

A value class. A ramp is an id, a display name, a kind and 2+ stops.

```cpp
// cwColorRamp.h
#ifndef CWCOLORRAMP_H
#define CWCOLORRAMP_H

// Our includes
#include "cwGlobals.h"

// Qt includes
#include <QColor>
#include <QString>
#include <QVector>

class CAVEWHERE_LIB_EXPORT cwColorRamp
{
public:
    enum class Kind {
        Sequential,   // one hue or an ordered perceptual sweep, dark at t = 0
        Diverging,    // two hues with white at t = 0.5
        Categorical   // stops are slots, no interpolation
    };

    struct Stop {
        double position = 0.0; // 0..1, ascending
        QColor color;
    };

    cwColorRamp() = default;
    cwColorRamp(const QString& id, const QString& name, Kind kind, const QVector<Stop>& stops);

    QString id() const { return m_id; }
    QString name() const { return m_name; }
    Kind kind() const { return m_kind; }
    const QVector<Stop>& stops() const { return m_stops; }
    bool isValid() const { return m_stops.size() >= 2; }

    // t is clamped to [0, 1]. Sequential/Diverging blend the two neighboring
    // stops in OKLab. Categorical returns stop[floor(t * stopCount)] unblended.
    QColor colorAt(double t) const;

    // count evenly spaced samples of colorAt, for the GPU block and the legend.
    // Categorical: sample i is stop[i % stopCount], so a slot index indexes it.
    QVector<QColor> bake(int count) const;

    static QVector<cwColorRamp> presets();
    static cwColorRamp preset(const QString& id); // invalid ramp when unknown
    static QString defaultDepthRampId();          // "depth"
    static QString defaultCategoricalRampId();    // "okabe-ito"

    // OKLab helpers, exposed so tests can check the round trip.
    struct Lab { double l = 0.0; double a = 0.0; double b = 0.0; };
    static Lab toOklab(const QColor& color);
    static QColor fromOklab(const Lab& lab);

private:
    QString m_id;
    QString m_name;
    Kind m_kind = Kind::Sequential;
    QVector<Stop> m_stops;
};

#endif
```

OKLab math (Björn Ottosson's published matrices). Use these constants exactly;
put them in an anonymous namespace in the `.cpp`:

```cpp
namespace {

double srgbToLinear(double c)
{
    return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
}

double linearToSrgb(double c)
{
    c = std::clamp(c, 0.0, 1.0);
    return c <= 0.0031308 ? 12.92 * c : 1.055 * std::pow(c, 1.0 / 2.4) - 0.055;
}

} // namespace

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

    return QColor::fromRgbF(linearToSrgb(r), linearToSrgb(g), linearToSrgb(b));
}
```

`colorAt(t)`: clamp t; find the last stop with `position <= t` and the next one;
`u = (t - a.position) / (b.position - a.position)`; blend `toOklab(a)` and
`toOklab(b)` component-wise by `u`; return `fromOklab`. Guard a zero-width pair.

Presets (hex values are named constants in the `.cpp`, one `constexpr const char*`
per stop is fine, or a static table):

| id | name | kind | stops (position: hex) |
|---|---|---|---|
| `depth` | Depth | Diverging | 0: `#1e1b4b`, 0.25: `#2563eb`, 0.5: `#ffffff`, 0.75: `#d9a066`, 1: `#7c3a0b` |
| `blue` | Blue | Sequential | 0: `#1e1b4b`, 0.5: `#2563eb`, 1: `#dbeafe` |
| `warm` | Warm | Sequential | 0: `#451a03`, 0.5: `#f59e0b`, 1: `#fef3c7` |
| `viridis` | Viridis | Sequential | 0: `#440154`, .125: `#482878`, .25: `#3e4989`, .375: `#31688e`, .5: `#26828e`, .625: `#1f9e89`, .75: `#35b779`, .875: `#6ece58`, 1: `#fde725` |
| `cividis` | Cividis | Sequential | 0: `#00224e`, .125: `#123570`, .25: `#3b496c`, .375: `#575d6d`, .5: `#707173`, .625: `#8a8678`, .75: `#a59c74`, .875: `#c3b369`, 1: `#fee838` |
| `rainbow` | Rainbow (classic) | Sequential | 0: `#0000ff`, .25: `#00ffff`, .5: `#00ff00`, .75: `#ffff00`, 1: `#ff0000` |
| `okabe-ito` | Okabe–Ito | Categorical | `#E69F00`, `#56B4E9`, `#009E73`, `#F0E442`, `#0072B2`, `#D55E00`, `#CC79A7`, `#999999` (positions i/7) |

### 1b. `cavewherelib/src/cwColorByUniform.h`

The POD both renderers upload. No `.cpp` needed.

```cpp
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

#endif
```

### 1c. `cavewherelib/src/cwColorBy.h/.cpp`

The QML-facing state. Owned by `cwRegionSceneManager`. Plain members with
explicit setters, `NOTIFY` signals, no bindables.

```cpp
class CAVEWHERE_LIB_EXPORT cwColorBy : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(ColorBy)
    QML_UNCREATABLE("ColorBy is owned by RegionSceneManager")

    Q_PROPERTY(Source source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(QString rampId READ rampId WRITE setRampId NOTIFY rampIdChanged)
    Q_PROPERTY(QStringList rampIds READ rampIds CONSTANT)
    Q_PROPERTY(double pivot READ pivot WRITE setPivot NOTIFY pivotChanged)
    Q_PROPERTY(double rangeMin READ rangeMin NOTIFY rangeChanged)
    Q_PROPERTY(double rangeMax READ rangeMax NOTIFY rangeChanged)
    Q_PROPERTY(double scrapStrength READ scrapStrength WRITE setScrapStrength NOTIFY scrapStrengthChanged)

public:
    enum class Source { None, Depth };   // Keyword is added in CP5
    Q_ENUM(Source)

    explicit cwColorBy(QObject* parent = nullptr);

    // Render objects the uniform is pushed to. Either may be null.
    void setTargets(cwRenderLinePlot* linePlot, cwRenderTexturedItems* items);

    Source source() const { return m_source; }
    void setSource(Source source);
    QString rampId() const { return m_rampId; }
    void setRampId(const QString& id);
    QStringList rampIds() const;                 // ids of non-categorical presets
    Q_INVOKABLE QString rampName(const QString& id) const;
    double pivot() const { return m_pivot; }
    void setPivot(double pivot);
    double rangeMin() const { return m_rangeMin; }
    double rangeMax() const { return m_rangeMax; }
    double scrapStrength() const { return m_scrapStrength; }
    void setScrapStrength(double strength);      // clamped 0..1

    // The legend's two questions: where does a value sit on the bar (0..1 using
    // the same rule as the shader), and what color is at that position.
    Q_INVOKABLE double normalized(double value) const;
    Q_INVOKABLE QColor colorAt(double t) const;

    // The block exactly as pushed to the renderers; tests read it back.
    cwColorByUniform uniform() const { return m_uniform; }

signals:
    void sourceChanged();
    void rampIdChanged();
    void pivotChanged();
    void rangeChanged();
    void scrapStrengthChanged();

private:
    void updateRangeFromLinePlot();
    void rebuildUniform();   // recompute m_uniform from state, then push()
    void push();             // linePlot->setColorBy(u); items->setSharedUniformBlock(bytes)
    void loadSettings();
    void saveSettings() const;

    QPointer<cwRenderLinePlot> m_linePlot;
    QPointer<cwRenderTexturedItems> m_items;
    Source m_source = Source::None;
    QString m_rampId;        // defaults to cwColorRamp::defaultDepthRampId()
    double m_pivot = 0.0;
    double m_rangeMin = 0.0;
    double m_rangeMax = 0.0;
    double m_scrapStrength = kDefaultScrapStrength; // 0.6, in the .cpp
    cwColorByUniform m_uniform;
};
```

`rebuildUniform()`:

1. `ramp = cwColorRamp::preset(m_rampId)`; if invalid fall back to the depth preset.
2. `samples = ramp.bake(kRampStops)` → fill `ramp[i] = {r, g, b, 1}` as floats.
3. `range = {min, max, pivot, halfExtent}` with `halfExtent = max(max - pivot, pivot - min)`.
4. `mode`: `Off` when `m_source == None` **or** the range is degenerate
   (`max <= min`, or `halfExtent <= 0` for diverging); otherwise `Diverging` when
   `ramp.kind() == Diverging`, else `Sequential`.
5. `params = {mode, scrapStrength, 0, WorldZ}`.
6. `push()`.

`updateRangeFromLinePlot()`: when the line plot exists and
`minZValue() <= maxZValue()`, copy them into `m_rangeMin/Max` and emit
`rangeChanged`; connect it to `cwRenderLinePlot::geometryChanged` in `setTargets`.
Then `rebuildUniform()`.

`normalized(value)` mirrors the GLSL rule from §1.2 for the current mode
(returns 0 when mode is Off). `colorAt(t)` is `ramp.colorAt(t)`.

Settings: `QSettings` keys `colorBy/source` (int), `colorBy/rampId`,
`colorBy/pivot`, `colorBy/scrapStrength`; load in the constructor, save in each
setter (same pattern as `cwRenderingSettings.cpp`).

### 1d. Hook into `cwRegionSceneManager`

- `#include "cwColorBy.h"`, member `cwColorBy* m_colorBy`, created in the
  constructor **after** `m_linePlot` and `m_items`, then
  `m_colorBy->setTargets(m_linePlot, m_items)`.
- `Q_PROPERTY(cwColorBy* colorBy READ colorBy CONSTANT)` and an inline getter.

### 1e. Stubs the renderers need now (filled in CP2/CP4)

- `cwRenderLinePlot`: `void setColorBy(const cwColorByUniform& uniform);`
  `cwColorByUniform colorBy() const;` backed by `cwTracked<cwColorByUniform>
  m_colorBy;`. The setter calls `m_colorBy.setValue(uniform); update();`.
- `cwRenderTexturedItems`: `void setSharedUniformBlock(const QByteArray& block);`
  `QByteArray sharedUniformBlock() const;` backed by
  `cwTracked<QByteArray> m_sharedUniformBlock;`. Setter: `setValue` then `update()`.

The RHI side ignores both until the next checkpoints.

### 1f. Tests

`testcases/test_cwColorRamp.cpp`, tag `[ColorBy]`:

- OKLab round trip: for `#000000`, `#ffffff`, `#ff0000`, `#2563eb`, `#d9a066`,
  `fromOklab(toOklab(c))` matches `c` within 1/255 per channel.
- `colorAt(0)` and `colorAt(1)` equal the first and last stop; `colorAt(0.5)` of the
  `depth` preset is white.
- Black→white two-stop ramp: lightness of `colorAt(t)` is monotonic for
  t = 0, 0.1, …, 1 (use `toOklab(...).l`).
- `bake(32)` has 32 entries; categorical `okabe-ito` baked sample 8 equals sample 0.
- `preset("nope")` is invalid; every id in `presets()` is valid and unique.

`testcases/test_cwColorBy.cpp`, tag `[ColorBy]`:

- Fresh `cwColorBy` (no targets): `uniform().params[0] == Off`.
- With a `cwRenderLinePlot` target (no scene needed): `setGeometry` with z from
  −500 to +100, `setSource(Depth)`, ramp `depth`, pivot 0 →
  `normalized(-500) == 0`, `normalized(0) == 0.5`, `normalized(100) == 0.6`
  (Approx), `uniform().range == {-500, 100, 0, 500}`, mode `Diverging`,
  `linePlot->colorBy() == colorBy.uniform()`.
- Ramp `blue` (sequential): `normalized(-500) == 0`, `normalized(100) == 1`.
- `setPivot(-200)` → `normalized(-200) == 0.5`, halfExtent 300.
- Degenerate geometry (one shot, flat z): mode is `Off`.
- Settings round trip: set values, construct a second `cwColorBy`, same values.
  Use a scoped `QSettings` organization/application name unique to the test
  (`QCoreApplication::applicationPid()` in the name) so parallel runs never collide,
  and clear it at the end.

Report: tests pass, nothing visual yet.

---

## Checkpoint 2: centerline colored by depth

### 2a. `cavewherelib/shaders/LinePlot.vert`

Add one varying after `vType`:

```glsl
layout(location = 1) out float vValue;
```

Set it on every path. In the early-return branches set `vValue = 0.0;`. On the
main path, right before `gl_Position = clip;`:

```glsl
// The world elevation of this corner, so depth coloring grades along a shot.
vValue = mix(iFrom.z, iTo.z, endSelect);
```

### 2b. `cavewherelib/shaders/LinePlot.frag`

Replace the body:

```glsl
#version 440 core

layout(location = 0) flat in uint vType; // 0 = centerline, non-zero = splay
layout(location = 1) in float vValue;    // world z (depth) or a per-segment value

layout(location = 0) out vec4 fragColor;

// Mirrors cwColorByUniform; keep the two in sync.
const int kRampStops = 32;
layout(std140, binding = 1) uniform ColorByBlock {
    vec4 ramp[kRampStops];
    vec4 range;   // rangeMin, rangeMax, pivot, halfExtent
    vec4 params;  // mode, strength, slotCount, valueSource
};

const float kModeOff = 0.0;
const float kModeSequential = 1.0;
const float kModeDiverging = 2.0;
const float kModeCategorical = 3.0;

const vec4 kCenterlineColor = vec4(1.0, 0.0, 0.0, 1.0);
const vec4 kSplayColor = vec4(1.0, 0.6, 0.6, 1.0);
// Splays keep reading as secondary under color-by: their ramp color is pulled
// this far toward white.
const float kSplayLighten = 0.5;

float normalizeValue(float v)
{
    float mode = params.x;
    if (mode == kModeDiverging) {
        return clamp(0.5 + 0.5 * (v - range.z) / range.w, 0.0, 1.0);
    }
    return clamp((v - range.x) / (range.y - range.x), 0.0, 1.0);
}

vec4 rampColor(float t)
{
    float x = clamp(t, 0.0, 1.0) * float(kRampStops - 1);
    int i = int(floor(x));
    int j = min(i + 1, kRampStops - 1);
    return mix(ramp[i], ramp[j], x - float(i));
}

vec4 slotColor(float slot)
{
    int i = clamp(int(slot + 0.5), 0, kRampStops - 1);
    return ramp[i];
}

void main(void)
{
    bool isSplay = vType != 0u;
    if (params.x == kModeOff) {
        fragColor = isSplay ? kSplayColor : kCenterlineColor;
        return;
    }

    vec4 color = params.x == kModeCategorical
        ? slotColor(vValue)
        : rampColor(normalizeValue(vValue));
    if (isSplay) {
        color.rgb = mix(color.rgb, vec3(1.0), kSplayLighten);
    }
    fragColor = vec4(color.rgb, 1.0);
}
```

The `cavewherelib_shaders_lineplot` CMake block already targets GLSL 150/300es;
nothing to change there.

### 2c. `cavewherelib/src/cwRHILinePlot.h/.cpp`

- Member `QRhiBuffer* m_colorByBuffer = nullptr;` and
  `cwTracked<cwColorByUniform> m_colorBy;`. Delete the buffer in the destructor.
- `initializeResources`: create it, **and upload the current value once**
  (`cwTracked` starts unchanged, so the first frame would otherwise read
  uninitialized memory):
  ```cpp
  const auto colorByBufferSize = rhi->ubufAligned(sizeof(cwColorByUniform));
  m_colorByBuffer = rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, colorByBufferSize);
  m_colorByBuffer->create();
  data.resourceUpdateBatch->updateDynamicBuffer(m_colorByBuffer, 0, sizeof(cwColorByUniform),
                                                &m_colorBy.value());
  ```
- `synchronize`: `m_colorBy = linePlot->m_colorBy; linePlot->m_colorBy.resetChanged();`
  (same shape as `m_data`).
- `updateResources`: when `m_colorBy.isChanged()`, `updateDynamicBuffer` the whole
  struct, then `resetChanged()`. Do this **before** `m_data.resetChanged()` or
  after, it does not matter, but do not nest it inside the `m_data.isChanged()` branch.
- Layout and SRB: add a second binding in both `createFn` and
  `ensureShaderResources`:
  ```cpp
  QRhiShaderResourceBinding::uniformBuffer(1, QRhiShaderResourceBinding::FragmentStage, m_colorByBuffer)
  ```
  (`nullptr` for the buffer in the layout, as the grid plane does.)
- `buildPipelineKey`: `key.perDrawBinding = 1; key.perDrawStages =
  cwShaderStageMask(cwRenderMaterialState::ShaderStage::Fragment); key.hasPerDraw = 1;`
  so the key describes the layout honestly.
- `reportGeometryBytes` can leave the UBO out; it is 544 bytes.

### 2d. Test: `test-qml/tst_OffscreenRenderColorBy.qml`

Copy the shape of `tst_OffscreenRenderSplays.qml` (project load, RHI wait and
skip, render-until-color loop). Two functions:

- `test_defaultStaysRed`: `RootData.regionSceneManager.colorBy.source = ColorBy.None`;
  render; `colorFraction(path, red, 70) > 0`.
- `test_depthRemovesRedAndAddsRamp`: set `source = ColorBy.Depth`, `rampId =
  "depth"`, `pivot = 0`; render; red fraction `== 0` (tolerance 40) **and** the
  image is non-uniform **and** at least one of white `(1,1,1)` or deep navy
  `(0.118, 0.106, 0.294)` has fraction `> 0` (Phake Cave 3000 starts at z = 0 and
  descends, so the top is near white and the bottom near navy).

Reset `source` to `None` in `cleanup()`.

Report with `$B/colorby_cp2.png`. Expect: centerline white near the entrance
fading to blue then navy with depth; splays paler.

---

## Checkpoint 3: chip, legend and popup

All of the color-by UI lives in the 3D view, bottom-left, opposite the scale bar
and compass. The Layers tab is untouched. One component, `ColorByLegend`, is
always present there and has two states: a compact **chip** when color-by is
off, and the **colorbar legend** when it is on. A single click (or right-click)
on either state opens a popup with the controls, anchored above it.

### 3a. `cavewherelib/qml/ColorByOptions.qml`

The popup's content. A `ColumnLayout` bound to
`RootData.regionSceneManager.colorBy`. Unchanged from the previous draft of
this plan except that it no longer embeds anywhere else.

```qml
import QtQuick as QQ
import QtQuick.Controls as QC
import QtQuick.Layouts
import cavewherelib

ColumnLayout {
    id: rootId
    objectName: "colorByOptions"

    readonly property ColorBy colorBy: RootData.regionSceneManager.colorBy
    readonly property bool active: colorBy.source !== ColorBy.None

    spacing: 4

    QC.Label {
        text: qsTr("Color by")
        font.pixelSize: Theme.fontSizeUI
    }

    QC.ComboBox {
        id: sourceComboId
        objectName: "colorBySourceCombo"
        Layout.fillWidth: true
        model: [qsTr("None"), qsTr("Depth")]
        currentIndex: rootId.colorBy.source === ColorBy.Depth ? 1 : 0
        onActivated: (index) => rootId.colorBy.source = index === 1 ? ColorBy.Depth : ColorBy.None
    }

    QC.ComboBox {
        id: rampComboId
        objectName: "colorByRampCombo"
        Layout.fillWidth: true
        visible: rootId.active
        model: rootId.colorBy.rampIds
        displayText: rootId.colorBy.rampName(currentText)
        delegate: QC.ItemDelegate {
            required property string modelData
            width: rampComboId.width
            text: rootId.colorBy.rampName(modelData)
        }
        currentIndex: rootId.colorBy.rampIds.indexOf(rootId.colorBy.rampId)
        onActivated: (index) => rootId.colorBy.rampId = rootId.colorBy.rampIds[index]
    }

    RowLayout {
        visible: rootId.active
        QC.Label { text: qsTr("White at") }
        QC.SpinBox {   // meters; from/to wide enough for real elevations
            objectName: "colorByPivotSpin"
            from: -10000; to: 10000
            value: Math.round(rootId.colorBy.pivot)
            onValueModified: rootId.colorBy.pivot = value
        }
        QC.Label { text: qsTr("m") }
    }

    RowLayout {
        visible: rootId.active
        QC.Label { text: qsTr("Scrap tint") }
        QC.Slider {
            objectName: "colorByStrengthSlider"
            Layout.fillWidth: true
            from: 0; to: 1
            value: rootId.colorBy.scrapStrength
            onMoved: rootId.colorBy.scrapStrength = value
        }
    }
}
```

Font sizes come from `Theme` tokens. The pivot SpinBox is in meters for now;
routing it through `RootData.settings.unitSettings` is a follow-up unless other
length fields in the view already do that.

### 3b. `cavewherelib/qml/ColorByPopup.qml`

Thin wrapper so the renderer can place it. Same precedent as
`MeasurementReadoutPopup`: a `QC.Popup` parented to the renderer item.

```qml
import QtQuick as QQ
import QtQuick.Controls as QC
import cavewherelib

QC.Popup {
    id: rootId
    objectName: "colorByPopup"

    readonly property int contentWidth: 260

    modal: false
    focus: true
    closePolicy: QC.Popup.CloseOnEscape | QC.Popup.CloseOnPressOutside
    padding: Theme.pageMargin

    contentItem: ColorByOptions {
        width: rootId.contentWidth
    }
}
```

Use whatever the other popups in `cavewherelib/qml` use for `padding` and
background; `Theme.pageMargin` exists and `RenderStatsHud` uses it. If the other
popups set a `background` explicitly, copy that.

### 3c. `cavewherelib/qml/ColorByLegend.qml`

Always visible. Two states driven by `colorBy.source`.

```qml
import QtQuick as QQ
import QtQuick.Controls as QC
import cavewherelib

QQ.Item {
    id: rootId
    objectName: "colorByLegend"

    readonly property ColorBy colorBy: RootData.regionSceneManager.colorBy
    readonly property bool active: colorBy.source !== ColorBy.None
    readonly property int stripCount: 48
    readonly property int barWidth: 14
    readonly property int barHeight: 160
    readonly property int labelWidth: 56
    readonly property real backdropOpacity: 0.8
    readonly property bool pivotInside: colorBy.rangeMin < colorBy.pivot && colorBy.pivot < colorBy.rangeMax

    signal clicked()

    // The bar covers only [rangeMin, rangeMax] of the ramp, so a +100 / -500
    // cave shows a short warm tail above a long cool run, not a symmetric bar.
    function valueAtStrip(i) {
        return colorBy.rangeMax - (colorBy.rangeMax - colorBy.rangeMin) * (i + 0.5) / stripCount
    }

    width: active ? legendId.width : chipId.width
    height: active ? legendId.height : chipId.height

    // A plain tap opens the controls; right-click too, for people who try it.
    // Nothing else here reacts to a press, so no gesture is being taken away.
    QQ.TapHandler {
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onTapped: rootId.clicked()
    }

    QQ.Rectangle {
        anchors.fill: parent
        radius: 6
        color: Theme.surface
        opacity: rootId.backdropOpacity
    }

    // Off state: a compact chip that names the feature, so there is always
    // something to click when no legend is showing.
    QQ.Row {
        id: chipId
        objectName: "colorByChip"
        visible: !rootId.active
        padding: 6
        spacing: 6

        QQ.Image {
            source: "qrc:/twbs-icons/icons/palette.svg"
            width: Theme.fontSizeUI
            height: width
            anchors.verticalCenter: parent.verticalCenter
        }
        QC.Label {
            text: qsTr("Color by: None")
            font.pixelSize: Theme.fontSizeSmall
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    // On state: the colorbar with min / pivot / max labels.
    QQ.Row {
        id: legendId
        visible: rootId.active
        padding: 6
        spacing: Theme.linkBarButtonSpacing

        QQ.Column {
            id: barId
            QQ.Repeater {
                model: rootId.stripCount
                QQ.Rectangle {
                    required property int index
                    width: rootId.barWidth
                    height: rootId.barHeight / rootId.stripCount
                    color: rootId.colorBy.colorAt(rootId.colorBy.normalized(rootId.valueAtStrip(index)))
                }
            }
        }

        QQ.Item {
            width: rootId.labelWidth
            height: rootId.barHeight

            QC.Label {
                anchors.top: parent.top
                text: Math.round(rootId.colorBy.rangeMax) + " m"
                font.pixelSize: Theme.fontSizeCaption
            }
            QC.Label {
                visible: rootId.pivotInside
                y: parent.height * (rootId.colorBy.rangeMax - rootId.colorBy.pivot)
                   / (rootId.colorBy.rangeMax - rootId.colorBy.rangeMin) - height / 2
                text: Math.round(rootId.colorBy.pivot) + " m"
                font.pixelSize: Theme.fontSizeCaption
            }
            QC.Label {
                anchors.bottom: parent.bottom
                text: Math.round(rootId.colorBy.rangeMin) + " m"
                font.pixelSize: Theme.fontSizeCaption
            }
        }
    }
}
```

`palette.svg` ships in `cavewherelib/twbs-icons/icons/`, so the chip icon path above resolves.

### 3d. Placement in `GLTerrainRenderer.qml`

Next to the existing bottom-right `Row` (scale bar + compass). First extract that
row's `20` into `readonly property int chromeMargin: 20` on the renderer root and
use it in both places.

```qml
ColorByLegend {
    id: colorByLegendId
    anchors.left: parent.left
    anchors.bottom: parent.bottom
    anchors.margins: rootId.chromeMargin
    z: rootId.zOverlay
    onClicked: colorByPopupId.open()
}

ColorByPopup {
    id: colorByPopupId
    parent: rendererId
    // Anchored above the legend's top-left corner, kept inside the view.
    x: colorByLegendId.x
    y: Math.max(0, colorByLegendId.y - height - Theme.linkBarButtonSpacing)
}
```

The legend sits at `zOverlay`, above `LeadView` and `LinePlotLabelView`, so the
tap never reaches their tap-away handlers or the turntable camera. Leave the
Layers tab and `KeywordTab.qml` untouched; the existing objectName chains stay valid.

### 3e. Test: `test-qml/tst_ColorByOptions.qml`

`MainWindowTest` + `TestCase`, `--platform offscreen`:

- Load Phake Cave 3000, `RootData.pageSelectionModel.gotoPageByName(null, "View")`.
- Find the legend by chain `rootId->viewPage->SplitView->renderer->colorByLegend`
  (`ObjectFinder.findObjectByChain`). Verify it is visible and that
  `colorByChip` under it is visible (off state).
- `mouseClick(legend)`; `tryVerify(() => popup.opened)` where the popup is
  `findChild(mainWindow, "colorByPopup")`.
- Find `colorBySourceCombo` under the popup, select index 1 the way
  `tst_KeywordTab.qml` drives a combo (or set `currentIndex` and call
  `activated(1)`), then
  `tryCompare(RootData.regionSceneManager.colorBy, "source", ColorBy.Depth)`.
- The chip becomes hidden and the colorbar rows appear (the `Repeater` has
  `stripCount` items).
- `keyClick(Qt.Key_Escape)` closes the popup; `tryVerify(() => !popup.opened)`.
- A right-click on the legend also opens it (`mouseClick(legend, x, y, Qt.RightButton)`).
- `cleanup()`: `source = ColorBy.None`, close the popup.

Report with a window screenshot if the platform has one, else the `$B/colorby_cp2.png`
from checkpoint 2 still shows the render; describe the chip and popup in words.

---

## Checkpoint 4: scrap tint

### 4a. Shared block in `cwRenderTexturedItems` / `cwRhiTexturedItems`

Per-draw payload today = `modelMatrix` (64 B) + `item.uniformBlock`. Make it
`modelMatrix` + **shared block** + `item.uniformBlock`. GLTF items declare only
the `mat4` in their shader and ignore the rest, so they are unaffected except for
a larger buffer.

GUI side (`cwRenderTexturedItems`): the `m_sharedUniformBlock` member and setter
from CP1 already exist. Add `friend class cwRhiTexturedItems` access (it is
already a friend).

Render side (`cwRhiTexturedItems`):

- Member `QByteArray m_sharedUniformBlock;`.
- In `synchronize(...)`, after the per-item command drain: if
  `source->m_sharedUniformBlock.isChanged()`, copy it, call `resetChanged()`, and
  for every item whose `material.wantsPerDrawUniform()` set
  `uniformNeedsUpdate = true`. (Find the item container with
  `grep -n "m_items" src/cwRhiTexturedItems.cpp`.)
- `Item::buildPerDrawUniformPayload()`: insert `owner->m_sharedUniformBlock`
  between the matrix and `uniformBlock`. `owner` is already a member.

The existing update order (`updateUniformBuffer` then
`createShaderResourceBindings`) already rebuilds the bindings when the buffer is
recreated at a new size, so growing the payload is safe.

### 4b. Scrap material in `cwScrapManager::attachScrap`

```cpp
cwRenderMaterialState state;
state.cullMode = cwRenderMaterialState::CullMode::None;
state.perDrawUniformBinding = kScrapPerDrawBinding;   // 2: binding 1 is the texture
state.perDrawUniformStages = cwRenderMaterialState::ShaderStage::Fragment;
m_scrapToRenderId.insert(scrap,
                         m_renderScraps->addItem({.material = state,
                                                  .uniformBlock = cwRenderTexturedItems::emptyItemBlock()}));
```

Add to `cwRenderTexturedItems`:

```cpp
// The per-item tail of the scrap per-draw block: vec4 item (x = color-by value).
// Items always carry it so the GPU buffer is never shorter than the GLSL block.
static QByteArray emptyItemBlock();          // 16 zero bytes
void setItemColorValue(uint32_t id, float value);  // writes item.x via setUniformBlock
```

`kScrapPerDrawBinding` is a `constexpr int` in the scrap manager's anonymous
namespace. Leave the LiDAR mesh material (`cwTriangulateLiDARTask`) alone in this
checkpoint; note it as a follow-up.

### 4c. Shaders

`scrap.vert`: add `layout(location = 1) out vec3 vWorldPos;` and set
`vWorldPos = vVertex;` (positions are already world space).

`scrap.frag`: delete the dead commented-out GLSL at the top of the file, then:

```glsl
#version 440 core

layout(location = 0) in vec2 vTexCoord;
layout(location = 1) in vec3 vWorldPos;
layout(location = 0) out vec4 fragColor;

layout(binding = 1) uniform sampler2D Texture;

// Per-draw block: the textured-items payload prefix (model matrix, unused by
// scraps), the shared cwColorByUniform, then the per-item tail.
const int kRampStops = 32;
layout(std140, binding = 2) uniform ScrapBlock {
    mat4 modelMatrix;
    vec4 ramp[kRampStops];
    vec4 range;   // rangeMin, rangeMax, pivot, halfExtent
    vec4 params;  // mode, strength, slotCount, valueSource
    vec4 item;    // x = per-item color-by value
};

const float kModeOff = 0.0;
const float kModeDiverging = 2.0;
const float kModeCategorical = 3.0;
const float kValueSourcePerElement = 1.0;
// Rec. 601 luma; the paper's brightness survives the recolor so ink stays dark.
const vec3 kLumaWeights = vec3(0.299, 0.587, 0.114);

float normalizeValue(float v) { /* same as LinePlot.frag */ }
vec4 rampColor(float t)       { /* same as LinePlot.frag */ }
vec4 slotColor(float slot)    { /* same as LinePlot.frag */ }

void main() {
    vec4 textureSample = texture(Texture, vTexCoord);
    // Make fully transparent texels show as white instead of black.
    vec3 paper = mix(vec3(1.0), textureSample.rgb, textureSample.a);

    if (params.x != kModeOff) {
        float value = params.w == kValueSourcePerElement ? item.x : vWorldPos.z;
        vec4 color = params.x == kModeCategorical
            ? slotColor(value)
            : rampColor(normalizeValue(value));
        float luma = dot(paper, kLumaWeights);
        paper = mix(paper, color.rgb * luma, params.y);
    }

    fragColor = vec4(paper, textureSample.a);
}
```

Both shaders are compiled by the default `qt_add_shaders` block. If qsb rejects
the `vec4[32]` array for the legacy `100es`/`120` targets, move `scrap.vert` and
`scrap.frag` into the `cavewherelib_shaders_lineplot` block (GLSL 150/300es),
which the line plot already needs for the same reason.

### 4d. Test

Extend `tst_OffscreenRenderColorBy.qml` with `test_scrapTintFollowsDepth`:
Phake Cave 3000 has scraps. Render with `source = None` and with `source =
Depth`, `scrapStrength = 1`. Assert the two PNGs differ (`!imagesEqual`) and that
pure white paper fraction (`(1,1,1)`, tolerance 8) is **lower** with depth on
than with it off, since tinted paper is no longer white below the pivot. Keep the
centerline assertions from CP2 passing.

Report with `$B/colorby_cp4_off.png` and `$B/colorby_cp4_on.png`.

---

## Checkpoint 5: categorical by keyword

### 5a. Per-element value plumbing

`cwVisibilityProxy` gains the color half, so every keyword target can take a value:

```cpp
public slots:
    void setVisible(bool visible);
    void setColorValue(float value);     // stores, then applyColorValue when changed
protected:
    virtual void applyVisible(bool visible) = 0;
    virtual void applyColorValue(float value) { Q_UNUSED(value); }
```

- `cwLinePlotTripVisibility::applyColorValue` → `m_linePlot->setRangeColorValue(m_range.start, m_range.count, value)`; `pushToTarget()` also re-pushes the color value after a solve.
- `cwRenderTexturedItemVisibility::applyColorValue` → `m_items->setItemColorValue(m_itemId, value)`.

`cwRenderLinePlot`: a fourth per-instance buffer.

- Authoring copy `QVector<float> m_colorValues` (one per vertex, reset to zeros
  in `setGeometry`), `cwTracked<QVector<float>> m_colorValuesTracked` is
  unnecessary: wrap the authoring copy in `cwTracked` directly and publish by
  copy-on-write exactly like `setRangeVisible` (write to a copy, `setValue`,
  `update()`). Name it `setRangeColorValue(int start, int count, float value)`.
- `cwRHILinePlot`: `m_valueBuffer` (Dynamic VertexBuffer), binding 3 with stride
  `sizeof(float)`, attribute `{ 3, 4, QRhiVertexInputAttribute::Float, 0 }`.
  Upload `values[2 * i]` per instance when changed, and whenever the segment count
  changes upload a zero buffer of the new size so the instance count and the
  buffer always agree (mirror `updateVisibilityBuffer`'s gate).
- `LinePlot.vert`: `layout(location = 4) in float iValue;` and
  `vValue = params.w == kValueSourcePerElement ? iValue : mix(iFrom.z, iTo.z, endSelect);`
  This needs the `ColorByBlock` in the **vertex** stage too. Bind it at binding 1
  for both stages (`VertexStage | FragmentStage`) in layout, SRB and key.

### 5b. `cavewherelib/src/cwKeywordColorBy.h/.cpp`

Owned by `cwColorBy`. Groups the global keyword item model by one key and hands
each proxy its slot.

- Members: a `cwKeywordGroupByKeyModel m_groups` with
  `setSourceModel(keywordItemModel)` and `setKey(key)`; `QStringList values()`
  (row values in model order, for the legend); `int slotCount()`.
- On `rowsInserted`, `rowsRemoved`, `modelReset` and `dataChanged` of
  `m_groups`: walk every row `i`, read `ObjectsRole`, `qobject_cast<cwVisibilityProxy*>`
  each object (skip others: point clouds, label groups, leads), call
  `setColorValue(i)`. The "Other" row (items lacking the key) gets slot
  `cwColorByUniform::kRampStops - 1`, which `cwColorRamp::bake` for a categorical
  ramp fills with neutral gray `#999999`.
- `cwColorBy`: `Source::Keyword` + `Q_PROPERTY(QString keywordKey ...)` +
  `void setKeywordItemModel(cwKeywordItemModel*)` (called from `cwRootData`'s
  constructor where both the scene manager and the keyword model exist) +
  `Q_PROPERTY(QStringList legendValues READ ... NOTIFY legendChanged)` and
  `Q_INVOKABLE QColor slotColor(int slot)`. In Keyword mode the uniform is
  `{mode Categorical, valueSource PerElement, slotCount}`, ramp `okabe-ito`.
- Slot stability: rows come from the unfiltered item model and are sorted by
  value, so toggling visibility never changes a slot. A newly added trip can
  shift later slots; acceptable.

### 5c. UI

- The popup's source combo gains the keys: `None, Depth` then every key from
  `RootData.keywordFilterPipelineModel.possibleKeys` except `Object id` and
  `File Name`. Selecting a key sets `source = Keyword` and `keywordKey = key`.
- Legend: when `source == Keyword` show a `Column` of swatch + `QC.Label` rows
  from `legendValues`, scrollable in a `QC.ScrollView` above 12 rows.

### 5d. Tests

- `testcases/test_cwKeywordColorBy.cpp` `[ColorBy]`: a `cwKeywordItemModel` with
  three items keyed `Cave=A`, `Cave=B`, `Cave=A` and one without `Cave`, each
  wrapping a test-local `cwVisibilityProxy` subclass that records the last applied
  value. Expect slots 0, 1, 0 and `kRampStops - 1`.
- `testcases/test_cwRenderLinePlotColorValues.cpp` `[ColorBy]`: `setGeometry` with
  two shots, `setRangeColorValue(2, 2, 3.0f)`, `colorValues() == {0,0,3,3}`;
  out-of-range calls are ignored; `setGeometry` resets to zeros.
- Offscreen: `source = Keyword, key = Cave` on Phake Cave 3000 removes the red and
  adds Okabe–Ito orange `#E69F00` (tolerance 40).

---

## Checkpoint 6: follow-up sources (outline only)

Each of these is one new value producer on the plumbing above. Plan them
separately when reached.

- **Date timeline.** Parse the trip `Date` keyword (`yyyy-MM-dd`) to a day
  ordinal; `cwKeywordColorBy` pushes it as the value with `Sequential` mode, ramp
  `warm`, range = min/max ordinal. Legend labels format the ordinal back to a date.
- **Loop / dead-end.** Bridge-finding DFS over `cwSurveyNetwork`; an edge on a
  cycle is "in loop". Needs a per-shot value, so `cwLinePlotGeometry::generate`
  must emit a parallel `QVector<float>` next to `points` and `cwRenderLinePlot::
  setGeometry` must accept it. Categorical, two slots.
- **Loop closure error.** Survex 3d v8 writes `img_ERROR_INFO` after each
  traverse (`img->E`, `H`, `V`, `n_legs`, `length`); `cwSurvex3DFileReader` drops
  it today. Keep a `(from, to) → error` map through `readNetworkAndLookup` and look
  it up per shot in `generate`, which still has station names. Sequential, ramp
  gray→red.
- **Point cloud elevation.** Same block in `PointCloud.frag`, value = world z.
- **LiDAR meshes.** Give `cwTriangulateLiDARTask`'s material the CP4 scrap
  settings so scans tint too.
- **Light/dark ramp pairs** in `Theme`-style tokens: on the light 3D background
  (`#92D7F8`) the white pivot on a 2 px centerline is low contrast. Candidate
  fix: a per-scheme pivot tint (`#f5f5f5` light / `#ffffff` dark) carried in the
  preset, or a 1 px dark outline on the line quad.
- **Map export.** The 2D centerline overlay (`cwCaptureCenterline`) draws its own
  pen; scraps export tinted for free because the tiles render the live scene.

## 3. Running this as a workflow

One agent per checkpoint, strictly in order; each gets this file's path and its
checkpoint number, builds, runs only the listed tests, writes the PNG, and stops
with a report naming the test log and PNG paths. After each checkpoint run
`/cavewhere-review-lite` on the diff, fix what it finds, then commit the checkpoint
before the next one starts. Only the fix-and-commit stage commits.
