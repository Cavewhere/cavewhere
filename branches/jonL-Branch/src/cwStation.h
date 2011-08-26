#ifndef cwStation_H
#define cwStation_H

//Qt includes
#include <QVector3D>
#include <QVariant>

class cwStation {

public:
    enum DataRoles {
        NameRole,
        LeftRole,
        RightRole,
        UpRole,
        DownRole,
        PositionRole
    };

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

    void setData(QVariant data, DataRoles role);
    QVariant data(DataRoles role) const;

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

inline void cwStation::setName(QString name) { Name = name;}
inline void cwStation::setLeft(QString left) { Left = left;}
inline void cwStation::setRight(QString right) { Right = right;}
inline void cwStation::setUp(QString up) { Up = up;}
inline void cwStation::setDown(QString down) { Down = down;}
inline void cwStation::setPosition(QVector3D position) { Position = position;}

#endif // cwStation_H
