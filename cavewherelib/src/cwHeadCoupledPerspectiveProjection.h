#ifndef CWHEADCOUPLEDPERSPECTIVEPROJECTION_H
#define CWHEADCOUPLEDPERSPECTIVEPROJECTION_H

//Qt includes
#include <QQmlEngine>
#include <QVector3D>

//Our includes
#include "cwAbstractProjection.h"
class cwAbstractHeadTracker;
class cwViewMatrixComposer;

class CAVEWHERE_LIB_EXPORT cwHeadCoupledPerspectiveProjection : public cwAbstractProjection
{
    Q_OBJECT
    QML_NAMED_ELEMENT(HeadCoupledPerspectiveProjection)

    Q_PROPERTY(cwAbstractHeadTracker* tracker READ tracker WRITE setTracker NOTIFY trackerChanged)
    Q_PROPERTY(double fieldOfView READ fieldOfView WRITE setFieldOfView NOTIFY fieldOfViewChanged)
    Q_PROPERTY(double screenWidthMeters READ screenWidthMeters WRITE setScreenWidthMeters NOTIFY screenWidthMetersChanged)
    Q_PROPERTY(double screenHeightMeters READ screenHeightMeters WRITE setScreenHeightMeters NOTIFY screenHeightMetersChanged)
    Q_PROPERTY(double sensitivity READ sensitivity WRITE setSensitivity NOTIFY sensitivityChanged)
    Q_PROPERTY(bool viewMatrixOffsetEnabled READ viewMatrixOffsetEnabled WRITE setViewMatrixOffsetEnabled NOTIFY viewMatrixOffsetEnabledChanged)
    Q_PROPERTY(double parallaxStrength READ parallaxStrength WRITE setParallaxStrength NOTIFY parallaxStrengthChanged)
    Q_PROPERTY(double translationScale READ translationScale WRITE setTranslationScale NOTIFY translationScaleChanged)

public:
    explicit cwHeadCoupledPerspectiveProjection(QObject* parent = nullptr);

    cwAbstractHeadTracker* tracker() const;
    void setTracker(cwAbstractHeadTracker* tracker);

    double fieldOfView() const;
    void setFieldOfView(double fov);

    double screenWidthMeters() const;
    void setScreenWidthMeters(double width);

    double screenHeightMeters() const;
    void setScreenHeightMeters(double height);

    double sensitivity() const;
    void setSensitivity(double sensitivity);

    bool viewMatrixOffsetEnabled() const;
    void setViewMatrixOffsetEnabled(bool enabled);

    double parallaxStrength() const;
    void setParallaxStrength(double strength);

    double translationScale() const;
    void setTranslationScale(double scale);

    cwViewMatrixComposer* viewMatrixComposer() const;
    void setViewMatrixComposer(cwViewMatrixComposer* composer);

signals:
    void trackerChanged();
    void fieldOfViewChanged();
    void screenWidthMetersChanged();
    void screenHeightMetersChanged();
    void sensitivityChanged();
    void viewMatrixOffsetEnabledChanged();
    void parallaxStrengthChanged();
    void translationScaleChanged();

protected:
    cwProjection calculateProjection() override;

private slots:
    void onEyePositionChanged(QVector3D eyePos);

private:
    cwAbstractHeadTracker* m_tracker = nullptr;
    cwViewMatrixComposer* m_viewMatrixComposer = nullptr;
    double m_fieldOfView = 55.0;
    double m_screenWidthMeters = 0.30;
    double m_screenHeightMeters = 0.19;
    double m_sensitivity = 0.65;
    bool m_viewMatrixOffsetEnabled = true;
    double m_parallaxStrength = 0.3;
    double m_translationScale = 1.0;
    double m_cachedDefaultEz = 0.0;
    double m_referenceEz = 0.0;
    QVector3D m_lastEyePos;

    void updateCachedDefaultEz();
};

inline cwAbstractHeadTracker* cwHeadCoupledPerspectiveProjection::tracker() const
{
    return m_tracker;
}

inline double cwHeadCoupledPerspectiveProjection::fieldOfView() const
{
    return m_fieldOfView;
}

inline double cwHeadCoupledPerspectiveProjection::screenWidthMeters() const
{
    return m_screenWidthMeters;
}

inline double cwHeadCoupledPerspectiveProjection::screenHeightMeters() const
{
    return m_screenHeightMeters;
}

inline double cwHeadCoupledPerspectiveProjection::sensitivity() const
{
    return m_sensitivity;
}

inline bool cwHeadCoupledPerspectiveProjection::viewMatrixOffsetEnabled() const
{
    return m_viewMatrixOffsetEnabled;
}

inline double cwHeadCoupledPerspectiveProjection::parallaxStrength() const
{
    return m_parallaxStrength;
}

inline double cwHeadCoupledPerspectiveProjection::translationScale() const
{
    return m_translationScale;
}

inline cwViewMatrixComposer* cwHeadCoupledPerspectiveProjection::viewMatrixComposer() const
{
    return m_viewMatrixComposer;
}

#endif // CWHEADCOUPLEDPERSPECTIVEPROJECTION_H
