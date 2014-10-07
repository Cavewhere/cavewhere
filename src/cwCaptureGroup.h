/**************************************************************************
**
**    Copyright (C) 2014 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#ifndef CWCAPTUREGROUP_H
#define CWCAPTUREGROUP_H

#include <QObject>
#include <QSignalMapper>

class cwViewportCapture;

class cwCaptureGroup : public QObject
{
    Q_OBJECT
public:
    explicit cwCaptureGroup(QObject *parent = 0);

    void addCapture(cwViewportCapture* capture);
    cwViewportCapture* capture(int index) const;
    bool contains(cwViewportCapture* capture);
    int numberOfCaptures() const;

signals:

public slots:

private:
    QList<cwViewportCapture*> Captures;
    QSignalMapper* ScaleMapper;
    QSignalMapper* PositionMapper;
    QSignalMapper* RotationMapper;

    void updateCaptureScale(const cwViewportCapture *fixedCapture, cwViewportCapture* catpureToUpdate);

private slots:
    void updateScalesFrom(QObject* capture);

};

/**
 * @brief cwCaptureGroup::contains
 * @param capture
 * @return Returns true if the catpure is in this group and false if it isn't
 */
inline bool cwCaptureGroup::contains(cwViewportCapture *capture)
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
inline cwViewportCapture *cwCaptureGroup::capture(int index) const
{
    return Captures.at(index);
}


#endif // CWCAPTUREGROUP_H
