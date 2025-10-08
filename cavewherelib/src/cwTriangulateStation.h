/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWTRIANGULATESTATION_H
#define CWTRIANGULATESTATION_H

//Qt includes
#include <QSharedData>
#include <QString>
#include <QPointF>
#include <QVector3D>

class cwTriangulateStation
{
public:
    cwTriangulateStation() = default;
    cwTriangulateStation(const QString& name,
                             QVector3D notePosition,
                         QVector3D position) :
        m_notePosition(notePosition),
        m_position(position),
        m_name(name)
    {

    }

    QVector3D notePosition() const;
    void setNotePosition(QVector3D position);

    QVector3D position() const;
    void setPosition(QVector3D position);

    QString name() const;
    void setName(QString name);


private:
    QVector3D m_notePosition;
    QVector3D m_position;
    QString m_name;

};

inline QVector3D cwTriangulateStation::notePosition() const {
    return m_notePosition;
}

inline void cwTriangulateStation::setNotePosition(QVector3D position) {
    m_notePosition = position;
}

inline QVector3D cwTriangulateStation::position() const {
    return m_position;
}

inline void cwTriangulateStation::setPosition(QVector3D position) {
    m_position = position;
}

inline QString cwTriangulateStation::name() const {
    return m_name;
}

inline void cwTriangulateStation::setName(QString name) {
    m_name = name;
}


#endif // CWTRIANGULATESTATION_H
