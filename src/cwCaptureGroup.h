/**************************************************************************
**
**    Copyright (C) 2014 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#ifndef CWCAPTUREGROUP_H
#define CWCAPTUREGROUP_H

//Qt includes
#include <QObject>
#include <QSignalMapper>
#include <QPointF>
#include <QHash>

class cwCaptureViewport;

class cwCaptureGroup : public QObject
{
    Q_OBJECT
public:
    explicit cwCaptureGroup(QObject *parent = 0);

    void addCapture(cwCaptureViewport* capture);
    void removeCapture(cwCaptureViewport* capture);
    int indexOfCapture(cwCaptureViewport* capture) const;
    cwCaptureViewport* capture(int index) const;
    bool contains(cwCaptureViewport* capture);
    int numberOfCaptures() const;

signals:

public slots:

private:
    class ViewportGroupData {
    public:
        ViewportGroupData() {}

        QPointF OldPosition; //The old paper position

        //This is unitless offset of the viewport. The offset set is first calculated in
        //paper units, and then divided by width(), height() to make it unitless. Unitless
        //offsets can the be expanded, even if the scale changes.
        QPointF Offset;
    };

    QList<cwCaptureViewport*> Captures;
    QHash<cwCaptureViewport*, ViewportGroupData> CaptureData;

    QSignalMapper* ScaleMapper;
    QSignalMapper* PositionMapper;
    QSignalMapper* RotationMapper;

    void updateCaptureScale(const cwCaptureViewport *fixedCapture, cwCaptureViewport* catpureToUpdate);
    void updateCaptureRotation(const cwCaptureViewport* fixedCapture, cwCaptureViewport* captureToUpdate);
    void updateCaptureTranslation(cwCaptureViewport* capture);
    void updateCaptureOffsetTranslation(cwCaptureViewport* capture);

    void initializePosition(cwCaptureViewport* capture);

    void updateViewportGroupData(cwCaptureViewport* capture, bool inRotation = false);

    cwCaptureViewport* primaryCapture() const;
    bool isCoplanerWithPrimaryCapture(cwCaptureViewport* capture) const;

    QTransform removeRotationTransform(cwCaptureViewport* capture) const;

private slots:
    void updateScalesFrom(QObject* capture);
    void updateRotationFrom(QObject* capture);
    void updateTranslationFrom(QObject* capture);

};

/**
 * @brief cwCaptureGroup::contains
 * @param capture
 * @return Returns true if the catpure is in this group and false if it isn't
 */
inline bool cwCaptureGroup::contains(cwCaptureViewport *capture)
{
    return Captures.contains(capture);
}

/**
 * @brief cwCaptureGroup::numberOfCaptures
 * @return Returns the number of catpures in this group
 */
inline int cwCaptureGroup::numberOfCaptures() const
{
    return Captures.size();
}

/**
 * @brief cwCaptureGroup::capture
 * @param index - The index of the capture. If the index is invalid this will assert
 * @return Returns the catpure at index
 */
inline int cwCaptureGroup::indexOfCapture(cwCaptureViewport *capture) const
{
    return Captures.indexOf(capture);
}

inline cwCaptureViewport *cwCaptureGroup::capture(int index) const
{
    return Captures.at(index);
}


#endif // CWCAPTUREGROUP_H
