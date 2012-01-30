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
    cwTriangulateStation();

    QPointF notePosition() const;
    void setNotePosition(QPointF position);

    QVector3D position() const;
    void setPosition(QVector3D position);

    QString name() const;
    void setName(QString name);


private:
    class PrivateData : public QSharedData {
    public:
        QPointF NotePosition;
        QVector3D Position;
        QString Name;
    };

    QSharedDataPointer<PrivateData> Data;

};

inline QPointF cwTriangulateStation::notePosition() const {
    return Data->NotePosition;
}

inline void cwTriangulateStation::setNotePosition(QPointF position) {
    Data->NotePosition = position;
}

inline QVector3D cwTriangulateStation::position() const {
    return Data->Position;
}

inline void cwTriangulateStation::setPosition(QVector3D position) {
    Data->Position = position;
}

inline QString cwTriangulateStation::name() const {
    return Data->Name;
}

inline void cwTriangulateStation::setName(QString name) {
    Data->Name = name;
}


#endif // CWTRIANGULATESTATION_H
