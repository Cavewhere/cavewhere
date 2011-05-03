#ifndef cwStation_H
#define cwStation_H

//Qt includes
#include <QObject>
#include <QDebug>
#include <QVector3D>

class cwStation {

public:
    cwStation();
    cwStation(const cwStation& station);
    cwStation(QString name);
    cwStation(QString name, float left, float right, float up, float down);

    QString name() const;
    QString left() const;
    QString right() const;
    QString up() const;
    QString down() const;
    QVector3D position() const;

    bool isValid() const { return !Name.isEmpty(); }

public slots:
    void setName(QString Name);
    void setLeft(QString left);
    void setRight(QString right);
    void setUp(QString up);
    void setDown(QString down);
    void setPosition(QVector3D position); //This is set by the loop closer

protected:
    QString Name;

    QString Left;
    QString Right;
    QString Up;
    QString Down;

    QVector3D Position;
};

inline QString cwStation::name() const { return Name; }
inline QString cwStation::left() const { return Left; }
inline QString cwStation::right() const { return Right; }
inline QString cwStation::up() const { return Up; }
inline QString cwStation::down() const { return Down; }
inline QVector3D cwStation::position() const { return Position; }

#endif // cwStation_H
