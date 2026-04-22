/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwHookFilterPreview.h"
#include "cwPenStrokeFilter.h"
#include "cwPenPoint.h"

//Qt includes
#include <QCanvasPainter>
#include <QCanvasPainterItemRenderer>
#include <QColor>
#include <QGuiApplication>
#include <QPainterPath>
#include <QPalette>
#include <QPointF>
#include <QRectF>
#include <QVector>

#include "cwSketchDrawCanvas.h"

namespace {

QVector<cwPenPoint> bakeStroke(std::initializer_list<QPointF> pts)
{
    QVector<cwPenPoint> v;
    v.reserve(static_cast<int>(pts.size()));
    for (const QPointF &p : pts) {
        v.append(cwPenPoint(p, 1.0, 0));
    }
    return v;
}

// Gallery of real iPad captures from the pen-filter test corpus. Each
// sample exercises a distinct hook pattern the user can visually compare:
// V-hooks at the head, L-shaped touchdown drifts, a V at the tail, and
// one clean stroke that should never be trimmed. Order here MUST match
// the label order in SketchSettingsItem.qml.
const QVector<QVector<cwPenPoint>> &samples()
{
    static const QVector<QVector<cwPenPoint>> kSamples = {
        bakeStroke({
                {-3.831344, 3.080256}, {-3.831344, 3.080256}, {-3.831344, 3.080256},
                {-3.831344, 3.077195}, {-3.831344, 3.077195},
                {-3.834609, 3.073522}, {-3.834609, 3.073522}, {-3.834609, 3.073522},
                {-3.834609, 3.070461}, {-3.834609, 3.070461}, {-3.834609, 3.070461},
                {-3.837874, 3.070461}, {-3.837874, 3.070461}, {-3.837874, 3.070461},
                {-3.837874, 3.070461}, {-3.837874, 3.070461}, {-3.837874, 3.070461},
                {-3.837874, 3.070461}, {-3.837874, 3.070461},
                {-3.834609, 3.067400}, {-3.834609, 3.067400},
                {-3.831344, 3.067400}, {-3.831344, 3.067400},
                {-3.821549, 3.067400}, {-3.821549, 3.067400},
                {-3.812570, 3.064339}, {-3.812570, 3.064339},
                {-3.802775, 3.064339}, {-3.802775, 3.064339},
                {-3.792980, 3.064339}, {-3.792980, 3.064339},
                {-3.780736, 3.064339}, {-3.780736, 3.064339},
                {-3.770941, 3.064339}, {-3.770941, 3.064339},
                {-3.757881, 3.064339}, {-3.757881, 3.064339},
                {-3.748086, 3.064339}, {-3.748086, 3.064339},
                {-3.739107, 3.064339}, {-3.739107, 3.064339},
                {-3.726047, 3.064339}, {-3.726047, 3.064339},
                {-3.716252, 3.064339}, {-3.716252, 3.064339},
                {-3.710538, 3.064339}, {-3.710538, 3.064339},
                {-3.700743, 3.064339}, {-3.700743, 3.064339},
            }),
        bakeStroke({
                {-1.627793, -0.911021}, {-1.627793, -0.911021}, {-1.627793, -0.911021},
                {-1.636364, -0.913030}, {-1.636364, -0.913030},
                {-1.640114, -0.913030}, {-1.640114, -0.913030},
                {-1.642256, -0.915038}, {-1.642256, -0.915038},
                {-1.644399, -0.917047}, {-1.644399, -0.917047},
                {-1.646542, -0.919458}, {-1.646542, -0.919458},
                {-1.648685, -0.921467}, {-1.648685, -0.921467},
                {-1.650827, -0.923475}, {-1.650827, -0.923475},
                {-1.650827, -0.925484}, {-1.650827, -0.925484}, {-1.650827, -0.925484},
                {-1.650827, -0.925484}, {-1.650827, -0.925484},
                {-1.650827, -0.927493}, {-1.650827, -0.927493}, {-1.650827, -0.927493},
                {-1.650827, -0.927493}, {-1.650827, -0.927493}, {-1.650827, -0.927493},
                {-1.650827, -0.927493}, {-1.650827, -0.927493}, {-1.650827, -0.927493},
                {-1.650827, -0.927493}, {-1.650827, -0.927493}, {-1.650827, -0.927493},
                {-1.650827, -0.927493}, {-1.650827, -0.927493}, {-1.650827, -0.927493},
                {-1.650827, -0.927493}, {-1.650827, -0.927493},
                {-1.648685, -0.927493}, {-1.648685, -0.927493},
                {-1.646542, -0.929904}, {-1.646542, -0.929904},
                {-1.642256, -0.931912}, {-1.642256, -0.931912},
                {-1.636364, -0.933921}, {-1.636364, -0.933921},
                {-1.634221, -0.935930}, {-1.634221, -0.935930},
                {-1.627793, -0.940349}, {-1.627793, -0.940349},
                {-1.621365, -0.942358}, {-1.621365, -0.942358},
                {-1.614937, -0.944367}, {-1.614937, -0.944367},
                {-1.606902, -0.946376}, {-1.606902, -0.946376},
                {-1.600474, -0.948384}, {-1.600474, -0.948384},
                {-1.591903, -0.950795}, {-1.591903, -0.950795},
                {-1.583868, -0.952804}, {-1.583868, -0.952804},
            }),
        bakeStroke({
                {-4.022268, 0.539324}, {-4.022268, 0.539324},
                {-4.024410, 0.543743}, {-4.024410, 0.543743},
                {-4.030839, 0.547760}, {-4.030839, 0.547760},
                {-4.035124, 0.549769}, {-4.035124, 0.549769},
                {-4.036731, 0.554189}, {-4.036731, 0.554189},
                {-4.041016, 0.558206}, {-4.041016, 0.558206},
                {-4.045302, 0.562625}, {-4.045302, 0.562625},
                {-4.047444, 0.566643}, {-4.047444, 0.566643},
                {-4.049587, 0.570661}, {-4.049587, 0.570661},
                {-4.051730, 0.577089}, {-4.051730, 0.577089},
                {-4.053873, 0.581106}, {-4.053873, 0.581106},
                {-4.053873, 0.587534}, {-4.053873, 0.587534},
                {-4.056015, 0.591552}, {-4.056015, 0.591552},
                {-4.056015, 0.597980}, {-4.056015, 0.597980},
                {-4.058158, 0.604408}, {-4.058158, 0.604408},
                {-4.058158, 0.608426}, {-4.058158, 0.608426},
                {-4.058158, 0.614854}, {-4.058158, 0.614854},
                {-4.059765, 0.618872}, {-4.059765, 0.618872},
                {-4.059765, 0.620880}, {-4.059765, 0.620880},
                {-4.059765, 0.625300}, {-4.059765, 0.625300},
                {-4.059765, 0.627308}, {-4.059765, 0.627308},
                {-4.061908, 0.629317}, {-4.061908, 0.629317}, {-4.061908, 0.629317},
                {-4.061908, 0.629317}, {-4.061908, 0.629317}, {-4.061908, 0.629317},
                {-4.061908, 0.625300}, {-4.061908, 0.625300},
                {-4.061908, 0.612443}, {-4.061908, 0.612443},
                {-4.061908, 0.599989}, {-4.061908, 0.599989},
                {-4.061908, 0.587534}, {-4.061908, 0.587534},
            }),
        bakeStroke({
                {0.071909, 1.143567}, {0.071909, 1.143567}, {0.071909, 1.143567},
                {0.071909, 1.143567}, {0.071909, 1.143567}, {0.071909, 1.143567},
                {0.071909, 1.141156}, {0.071909, 1.141156},
                {0.069766, 1.141156}, {0.069766, 1.141156},
                {0.067623, 1.139147}, {0.067623, 1.139147},
                {0.065481, 1.137139}, {0.065481, 1.137139},
                {0.061731, 1.135130}, {0.061731, 1.135130},
                {0.059588, 1.133121}, {0.059588, 1.133121},
                {0.055303, 1.128702}, {0.055303, 1.128702},
                {0.053160, 1.126693}, {0.053160, 1.126693},
                {0.051017, 1.122675}, {0.051017, 1.122675},
                {0.046732, 1.118256}, {0.046732, 1.118256},
                {0.044589, 1.116247}, {0.044589, 1.116247},
                {0.040304, 1.112230}, {0.040304, 1.112230},
                {0.038697, 1.105802}, {0.038697, 1.105802},
                {0.034411, 1.101784}, {0.034411, 1.101784},
                {0.030126, 1.097365}, {0.030126, 1.097365},
                {0.023698, 1.091338}, {0.023698, 1.091338},
                {0.017270, 1.084910}, {0.017270, 1.084910},
                {0.011377, 1.080893}, {0.011377, 1.080893},
                {0.004949, 1.072456}, {0.004949, 1.072456},
                {-0.003622, 1.066028}, {-0.003622, 1.066028},
                {-0.011657, 1.060001}, {-0.011657, 1.060001},
                {-0.022370, 1.051564}, {-0.022370, 1.051564},
                {-0.032548, 1.045136}, {-0.032548, 1.045136},
                {-0.038976, 1.039110},
            }),
    };
    return kSamples;
}

const QVector<cwPenPoint> &sampleStroke(int index)
{
    const auto &all = samples();
    static const QVector<cwPenPoint> empty;
    if (index < 0 || index >= all.size()) {
        return empty;
    }
    return all[index];
}

class cwHookFilterPreviewRenderer final : public QCanvasPainterItemRenderer
{
public:
    void synchronize(QCanvasPainterItem *item) override
    {
        auto *preview = qobject_cast<cwHookFilterPreview *>(item);
        if (preview == nullptr) {
            m_valid = false;
            return;
        }
        m_valid           = true;
        m_filtered        = preview->filtered();
        m_maxHookLength   = preview->maxHookLength();
        m_maxHookFraction = preview->maxHookFraction();
        m_sampleIndex     = preview->sampleIndex();

        const QPalette pal = QGuiApplication::palette();
        m_rawColor       = pal.color(QPalette::Disabled, QPalette::WindowText);
        m_filteredColor  = pal.color(QPalette::Active,   QPalette::Highlight);
        m_markerColor    = pal.color(QPalette::Active,   QPalette::HighlightedText);
        m_backgroundColor = pal.color(QPalette::Active,  QPalette::Base);
    }

    void paint(QCanvasPainter *painter) override
    {
        const float w = width();
        const float h = height();
        if (!m_valid || w <= 0.0f || h <= 0.0f) {
            return;
        }

        painter->clearRect(0.0f, 0.0f, w, h);
        painter->setRenderHint(QCanvasPainter::RenderHint::Antialiasing);

        const QVector<cwPenPoint> &raw = sampleStroke(m_sampleIndex);
        if (raw.size() < 2) {
            return;
        }

        cwPenStrokeFilter::Params params;
        params.maxHookLength   = m_maxHookLength;
        params.maxHookFraction = m_maxHookFraction;
        const QVector<cwPenPoint> filtered = cwPenStrokeFilter::trimHooks(raw, params);

        QRectF bbox(raw.first().position, raw.first().position);
        for (const auto &p : raw) {
            const QPointF &pt = p.position;
            bbox.setLeft(std::min(bbox.left(),   pt.x()));
            bbox.setRight(std::max(bbox.right(), pt.x()));
            bbox.setTop(std::min(bbox.top(),     pt.y()));
            bbox.setBottom(std::max(bbox.bottom(), pt.y()));
        }

        const double pad = 14.0;
        const double availW = std::max(1.0, double(w) - 2.0 * pad);
        const double availH = std::max(1.0, double(h) - 2.0 * pad);
        const double bw = std::max(bbox.width(),  1e-9);
        const double bh = std::max(bbox.height(), 1e-9);
        const double scale = std::min(availW / bw, availH / bh);

        const double drawnW = bw * scale;
        const double drawnH = bh * scale;
        const double offX = pad + (availW - drawnW) * 0.5 - bbox.left() * scale;
        const double offY = pad + (availH - drawnH) * 0.5 - bbox.top()  * scale;

        const auto toScreen = [&](const QPointF &p) {
            return QPointF(p.x() * scale + offX, p.y() * scale + offY);
        };

        const auto buildPath = [&](const QVector<cwPenPoint> &pts) {
            QPainterPath path;
            if (pts.isEmpty()) {
                return path;
            }
            path.moveTo(toScreen(pts.first().position));
            for (int i = 1; i < pts.size(); ++i) {
                path.lineTo(toScreen(pts[i].position));
            }
            return path;
        };

        cwSketchDrawCanvas draw(painter);

        // Background
        {
            QPainterPath bg;
            bg.addRect(0, 0, w, h);
            draw.setFillBrush(m_backgroundColor);
            draw.fillPath(bg);
        }

        const bool didTrim = m_filtered && filtered.size() < raw.size() && !filtered.isEmpty();

        // Raw stroke — always drawn. When we trimmed something, fade it so
        // the accent-colored kept portion pops; otherwise draw it in the
        // accent color (nothing was trimmed, so the "kept" stroke IS the
        // raw stroke).
        QColor rawColor = didTrim ? m_rawColor : m_filteredColor;
        rawColor.setAlphaF(didTrim ? 0.55 : 1.0);
        draw.setStrokePen(rawColor, didTrim ? 1.5 : 3.0,
                          Qt::RoundCap, Qt::RoundJoin);
        draw.strokePath(buildPath(raw));

        if (didTrim) {
            draw.setStrokePen(m_filteredColor, 3.0, Qt::RoundCap, Qt::RoundJoin);
            draw.strokePath(buildPath(filtered));

            const QPointF marker = toScreen(filtered.first().position);
            const double r = 4.5;
            QPainterPath dot;
            dot.addEllipse(marker, r, r);
            draw.setFillBrush(m_markerColor);
            draw.fillPath(dot);
            draw.setStrokePen(m_filteredColor, 1.5, Qt::RoundCap, Qt::RoundJoin);
            draw.strokePath(dot);
        }
    }

private:
    bool m_valid = false;
    bool m_filtered = false;
    int m_sampleIndex = 0;
    double m_maxHookLength = 0.050;
    double m_maxHookFraction = 0.15;
    QColor m_rawColor;
    QColor m_filteredColor;
    QColor m_markerColor;
    QColor m_backgroundColor;
};

} // namespace

cwHookFilterPreview::cwHookFilterPreview(QQuickItem *parent)
    : QCanvasPainterItem(parent)
{
    setFillColor(Qt::transparent);
    setAlphaBlending(true);
}

void cwHookFilterPreview::setMaxHookLength(double meters)
{
    if (m_maxHookLength == meters) {
        return;
    }
    m_maxHookLength = meters;
    emit maxHookLengthChanged();
    update();
}

void cwHookFilterPreview::setMaxHookFraction(double fraction)
{
    if (m_maxHookFraction == fraction) {
        return;
    }
    m_maxHookFraction = fraction;
    emit maxHookFractionChanged();
    update();
}

void cwHookFilterPreview::setFiltered(bool on)
{
    if (m_filtered == on) {
        return;
    }
    m_filtered = on;
    emit filteredChanged();
    update();
}

void cwHookFilterPreview::setSampleIndex(int index)
{
    if (m_sampleIndex == index) {
        return;
    }
    m_sampleIndex = index;
    emit sampleIndexChanged();
    update();
}

QCanvasPainterItemRenderer *cwHookFilterPreview::createItemRenderer() const
{
    return new cwHookFilterPreviewRenderer();
}
