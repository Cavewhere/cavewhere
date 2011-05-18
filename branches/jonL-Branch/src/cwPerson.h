#ifndef CWPERSON_H
#define CWPERSON_H

#include <QObject>

class cwPerson : public QObject
{
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)

    Q_OBJECT
public:
    explicit cwPerson(QObject *parent = 0);

    void setName(QString name);
    QString name() const;

signals:
    void nameChanged(QString name);

public slots:

private:
    QString Name;

};

inline QString cwPerson::name() const {
    return Name;
}

#endif // CWPERSON_H
