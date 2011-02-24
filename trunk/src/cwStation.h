#ifndef cwStation_H
#define cwStation_H

//Qt includes
#include <QObject>
#include <QDebug>
#include <QVector3D>

class cwStation : public QObject {
    Q_OBJECT

public:
    cwStation();
    cwStation(const cwStation& station);
    cwStation(QString name);
    cwStation(QString name, float left, float right, float up, float down);

    Q_INVOKABLE QString name() const;
    Q_INVOKABLE QString left() const;
    Q_INVOKABLE QString right() const;
    Q_INVOKABLE QString up() const;
    Q_INVOKABLE QString down() const;
    Q_INVOKABLE QVector3D position() const;

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

signals:
    void nameChanged();
    void leftChanged();
    void rightChanged();
    void upChanged();
    void downChanged();
    void positionChanged();
};

inline QString cwStation::name() const { return Name; }
inline QString cwStation::left() const { return Left; }
inline QString cwStation::right() const { return Right; }
inline QString cwStation::up() const { return Up; }
inline QString cwStation::down() const { return Down; }
inline QVector3D cwStation::position() const { return Position; }

#endif // cwStation_H
