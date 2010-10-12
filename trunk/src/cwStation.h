#ifndef cwStation_H
#define cwStation_H

//Qt includes
#include <QObject>
#include <QDebug>
#include <QVariant>

class cwStation : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString Name READ GetName WRITE SetName NOTIFY NameChanged);
    Q_PROPERTY(QVariant Left  READ GetLeft WRITE SetLeft NOTIFY LeftChanged);
    Q_PROPERTY(QVariant Right READ GetRight WRITE SetRight NOTIFY RightChanged);
    Q_PROPERTY(QVariant Up READ GetUp WRITE SetUp NOTIFY UpChanged);
    Q_PROPERTY(QVariant Down READ GetDown WRITE SetDown NOTIFY DownChanged);

public:
    cwStation();
    cwStation(QString name);
    cwStation(QString name, float left, float right, float up, float down);

    Q_INVOKABLE QString GetName() const;
    Q_INVOKABLE QVariant GetLeft() const;
    Q_INVOKABLE QVariant GetRight() const;
    Q_INVOKABLE QVariant GetUp() const;
    Q_INVOKABLE QVariant GetDown() const;

    bool IsValid() const { return !Name.isEmpty(); }

public slots:
    void SetName(QString Name);
    void SetLeft(QVariant left);
    void SetRight(QVariant right);
    void SetUp(QVariant up);
    void SetDown(QVariant down);

protected:
    QString Name;

    QVariant Left;
    QVariant Right;
    QVariant Up;
    QVariant Down;

signals:
    void NameChanged();
    void LeftChanged();
    void RightChanged();
    void UpChanged();
    void DownChanged();
};

inline QString cwStation::GetName() const { return Name; }
inline QVariant cwStation::GetLeft() const { return Left; }
inline QVariant cwStation::GetRight() const { return Right; }
inline QVariant cwStation::GetUp() const { return Up; }
inline QVariant cwStation::GetDown() const { return Down; }



#endif // cwStation_H
