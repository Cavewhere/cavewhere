#ifndef CWFUTURE_H
#define CWFUTURE_H

//Qt includes
#include <QFuture>
#include <QString>

class cwFuture {
public:
    cwFuture() {}
    cwFuture(const QFuture<void>& future, const QString& jobName) :
        mFuture(future),
        mName(jobName)
    { }

    QFuture<void> future() const { return mFuture; }
    QString name() const { return mName; }

private:
    QFuture<void> mFuture;
    QString mName;
};

#endif // CWFUTURE_H
