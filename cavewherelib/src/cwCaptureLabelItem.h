/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWCAPTURELABELITEM_H
#define CWCAPTURELABELITEM_H

// Qt includes
#include <QFont>
#include <QFontMetrics>
#include <QGraphicsItem>
#include <QPainterPath>
#include <QPen>
#include <QVector>
#include <QVector3D>

// Our includes
#include "cwGlobals.h"
#include "cwCaptureLabelPlacer.h"
#include "cwLabelPlacementControl.h"

class cwCamera;

/**
 * Shared base of the capture-scene annotation items that place text through
 * cwCaptureLabelPlacer (cwCaptureCenterline, cwCaptureLeads). Owns the
 * camera/viewport/scale/DPI setter surface both subclasses configured
 * identically, the label font and pen, and the request-building /
 * placement-application skeleton, so the two implementations cannot drift
 * (before this class they already had: the anchor sort ran in different
 * phases on different threads).
 */
class CAVEWHERE_LIB_EXPORT cwCaptureLabelItem : public QGraphicsItem
{
public:
    // Per-entry label state shared by every subclass entry type. A subclass
    // entry struct inherits this, adding its extra per-entry data, so the
    // buildRequests()/applyPlacementsTo() templates below treat all entry
    // types uniformly. Shadow resetPlacement()/applyPlacement() in a derived
    // entry to reset/apply the extra state — the templates run on the derived
    // type, so the shadowing resolves statically.
    struct LabelDrawData {
        QString text;
        QPointF anchor;
        QRectF  labelRect;

        void resetPlacement()
        {
            labelRect = QRectF();
        }

        void applyPlacement(const cwCaptureLabelPlacer::Placement& placement)
        {
            if(placement.placed) {
                // The placer returned a rect tightly sized to glyph ink;
                // paint() maps it back to the baseline-left draw point, so
                // storing it as-is reproduces exactly what was reserved.
                labelRect = placement.labelRect;
            }
        }
    };

    explicit cwCaptureLabelItem(QGraphicsItem* parent = nullptr);

    void setCamera(cwCamera* camera);
    void setViewport(const QRect& viewport);
    void setImageScale(double scale);
    void setExportDpi(int dpi);

    // Scale factor for converting paper-pixels-at-export-DPI into the item's
    // local coord system. Lets a single 300-DPI-paper-px constant produce the
    // same scene-inch size in both preview and full-res paths.
    void setPaperPxToLocal(double scale);

    // The font labels are measured and painted with (its point size; the
    // export DPI converts it to pixels via scaledLabelFont). Defaults to the
    // subclass constructor's choice; tests inject a known fixture font here
    // instead of mirroring that private constant. Takes effect on the next
    // buildLabelRequests()/paint().
    void setLabelFont(const QFont& font);

    QRectF boundingRect() const override;
    QPainterPath shape() const override;

protected:
    // Rebuilds the subclass's draw data from the camera/viewport/data
    // sources; called whenever one of the shared inputs above changes.
    // Implementations must call clearRequestIndex() (stale indices point into
    // the old entry vector) and finish by sorting entries with anchorOrder.
    virtual void rebuildGeometry() = 0;

    // See rebuildGeometry(): indices from a previous buildRequests() point
    // into the old entry vector; applying placements through them after a
    // rebuild would write the wrong entries' label rects.
    void clearRequestIndex()
    {
        m_requestIndex.clear();
    }

    QFont scaledLabelFont() const;

    // Projects a world position into the item's local paper coordinates.
    QPointF projectToLocal(const QVector3D& world) const;

    // Stable placement order shared by both subclasses: top-to-bottom,
    // left-to-right. Sorting in rebuildGeometry keeps placements
    // deterministic across rebuilds and exports.
    static bool anchorOrder(const LabelDrawData& a, const LabelDrawData& b)
    {
        if(a.anchor.y() != b.anchor.y()) {
            return a.anchor.y() < b.anchor.y();
        }
        return a.anchor.x() < b.anchor.x();
    }

    // The shared body of the subclasses' buildLabelRequests(): polls the
    // control once per entry (measuring tens of thousands of labels takes
    // real time on a whole-cave export, so a canceled run bails here too),
    // resets stale placement state, culls off-viewport anchors BEFORE the
    // expensive QPainterPath::addText measurement, and records each emitted
    // request's entry index so applyPlacementsTo() can map results back.
    // Text is measured with the same scaled font paint() uses, so the
    // placer's reserved rect matches the painter's rendered glyph rect.
    //
    // The viewport is the same struct handed to the placer's
    // setPlacementViewport — sharing one instance is what guarantees the
    // pre-measure cull drops only a subset of the placer's own cull (via
    // labelSizeUpperBound), keeping placements bit-identical with or without
    // it. An empty viewport.bounds disables the cull.
    //
    // leaderObstacleThickness/leaderMinLength are stamped on every request;
    // zero (the default) means placeAll ignores them.
    template<class Entry>
    QVector<cwCaptureLabelPlacer::LabelRequest> buildRequests(
        QVector<Entry>& entries,
        const cwLabelPlacementControl& control,
        const cwCaptureLabelPlacer::PlacementViewport& viewport,
        qreal leaderObstacleThickness = 0.0,
        qreal leaderMinLength = 0.0)
    {
        m_requestIndex.clear();

        QVector<cwCaptureLabelPlacer::LabelRequest> requests;
        if(entries.isEmpty()) {
            return requests;
        }

        const QFont placementFont = scaledLabelFont();
        const QFontMetricsF placementMetrics(placementFont);
        const bool cullToViewport = !viewport.bounds.isEmpty();

        requests.reserve(entries.size());
        m_requestIndex.reserve(entries.size());
        for(int i = 0; i < entries.size(); i++) {
            if(control.isCanceled && control.isCanceled()) {
                break;
            }

            Entry& entry = entries[i];
            entry.resetPlacement();
            if(entry.text.isEmpty()) {
                continue;
            }

            // Pre-measure viewport cull: anchors the placer would cull anyway
            // skip the QPainterPath::addText below. The upper-bound size
            // guarantees this drops only a subset of the placer's own cull,
            // so placements are unchanged.
            if(cullToViewport) {
                const QSizeF sizeBound = cwCaptureLabelPlacer::labelSizeUpperBound(
                    placementMetrics, entry.text.length());
                if(!cwCaptureLabelPlacer::viewportCullRect(viewport.bounds, sizeBound,
                                                           viewport.labelMargin)
                        .contains(entry.anchor)) {
                    continue;
                }
            }

            QPainterPath path;
            path.addText(QPointF(0.0, 0.0), placementFont, entry.text);
            const QRectF tightInk = path.boundingRect();
            if(tightInk.isEmpty()) {
                continue;
            }

            cwCaptureLabelPlacer::LabelRequest request{
                entry.text,
                entry.anchor,
                tightInk.size()
            };
            request.leaderObstacleThickness = leaderObstacleThickness;
            request.leaderMinLength = leaderMinLength;
            requests.append(request);
            m_requestIndex.append(i);
        }
        return requests;
    }

    // Applies placeAll's results for the requests built by the last
    // buildRequests() call, mapping each placement back to its entry.
    template<class Entry>
    void applyPlacementsTo(QVector<Entry>& entries,
                           const QVector<cwCaptureLabelPlacer::Placement>& placements)
    {
        Q_ASSERT(placements.size() == m_requestIndex.size());
        const int count = qMin(placements.size(), m_requestIndex.size());
        for(int i = 0; i < count; i++) {
            entries[m_requestIndex.at(i)].applyPlacement(placements.at(i));
        }
    }

    cwCamera* m_camera = nullptr;
    QRect  m_viewport;
    QRectF m_boundingRect;
    qreal  m_imageScale = 1.0;
    int    m_exportDpi = 96;
    double m_paperPxToLocal = 1.0;
    QFont  m_labelFont;
    QPen   m_labelPen;

private:
    // Maps each request from the last buildRequests() call back to its entry
    // index (empty-text and culled entries produce no request). Cleared on
    // rebuilds: indices into a rebuilt entry vector would write the wrong
    // entries' label rects.
    QVector<int> m_requestIndex;
};

#endif // CWCAPTURELABELITEM_H
