/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWRENDERLINEPLOT_H
#define CWRENDERLINEPLOT_H

// Our includes
#include "cwRenderObject.h"
#include "cwTracked.h"

// Qt includes
#include <QVector3D>
#include <QVector>
#include <QQmlEngine>

class cwRenderLinePlot : public cwRenderObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(RenderLinePlot)

    friend class cwRHILinePlot;

public:
    explicit cwRenderLinePlot(QObject *parent = nullptr);

    // tripCount sizes the per-trip visibility vector (the running-id space,
    // including geometry-less trips). New geometry remaps trip ids, so this
    // resets visibility to all-visible; the owner re-applies any hidden trips.
    void setGeometry(QVector<QVector3D> pointData,
                     QVector<unsigned int> indexData,
                     QVector<quint32> tripIds = {},
                     int tripCount = 0);

    // Per-trip visibility, one byte per running trip id (255 = visible,
    // 0 = hidden). Sampled in the vertex shader keyed by the vertex's tripId.
    // Tracked independently of the geometry so a keyword toggle re-uploads only
    // the small visibility texture, not the vertex/index buffers. Mirrors the
    // incremental cwRenderTexturedItems::setVisible(id, bool) used by scraps.
    void setTripVisible(int tripId, bool visible);

    float maxZValue() const;
    float minZValue() const;

    QVector<QVector3D> points() const;
    QVector<unsigned int> indexes() const;
    QVector<quint32> tripIds() const;
    QVector<quint8> tripVisibility() const;

protected:
    virtual cwRHIObject* createRHIObject() override;

private:
    struct Data {
        float maxZValue = 0.0;
        float minZValue = 0.0;

        QVector<QVector3D> points;
        QVector<unsigned int> indexes;
        QVector<quint32> tripIds;

        // This is needed for cwTracked to work, all ways changes
        bool operator!=(const Data& other) const { return true; }
    };

    cwTracked<Data> m_data;
    cwTracked<QVector<quint8>> m_tripVisibility;

public:
    // R8 visibility-texture sentinels: one byte per running trip id, read in the
    // vertex shader. Shared with cwRHILinePlot (its empty-geometry fallback must
    // agree on which value means "visible").
    static constexpr quint8 kTripVisible = 0xFF;
    static constexpr quint8 kTripHidden = 0x00;
};

inline float cwRenderLinePlot::maxZValue() const
{
    return m_data.value().maxZValue;
}

inline float cwRenderLinePlot::minZValue() const
{
    return m_data.value().minZValue;
}

inline QVector<QVector3D> cwRenderLinePlot::points() const
{
    return m_data.value().points;
}

inline QVector<unsigned int> cwRenderLinePlot::indexes() const
{
    return m_data.value().indexes;
}

inline QVector<quint32> cwRenderLinePlot::tripIds() const
{
    return m_data.value().tripIds;
}

inline QVector<quint8> cwRenderLinePlot::tripVisibility() const
{
    return m_tripVisibility.value();
}

#endif // CWRENDERLINEPLOT_H
