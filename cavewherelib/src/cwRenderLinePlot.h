/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWRENDERLINEPLOT_H
#define CWRENDERLINEPLOT_H

// Our includes
#include "cwGLObject.h"
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

    void setGeometry(QVector<QVector3D> pointData, QVector<unsigned int> indexData);

    float maxZValue() const;
    float minZValue() const;

    QVector<QVector3D> points() const;
    QVector<unsigned int> indexes() const;

protected:
    virtual cwRHIObject* createRHIObject() override;

private:
    struct Data {
        float maxZValue = 0.0;
        float minZValue = 0.0;

        QVector<QVector3D> points;
        QVector<unsigned int> indexes;

        // This is needed for cwTracked to work, all ways changes
        bool operator!=(const Data& other) const { return true; }
    };

    cwTracked<Data> m_data;
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

#endif // CWRENDERLINEPLOT_H
