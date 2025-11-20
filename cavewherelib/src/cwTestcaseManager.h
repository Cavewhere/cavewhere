/**************************************************************************
**
**    Copyright (C) 2016 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#ifndef CWTESTCASEMANAGER_H
#define CWTESTCASEMANAGER_H

#include <QObject>
#include <QAbstractListModel>
#include <QProcess>
#include <QQmlEngine>

/**
 * @brief The cwTestcaseManager class
 *
 * This class runs cavewhere-test (test case application) and allow the cavewhere-test
 * output to be displayed in qml inside the application.
 */
class cwTestcaseManager : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(TestcaseManager)

    Q_PROPERTY(QString result READ result NOTIFY resultChanged)
    Q_PROPERTY(QString arguments READ arguments WRITE setArguments NOTIFY argumentsChanged)
    Q_PROPERTY(ProcessState processState READ processState NOTIFY processStateChanged)

public:
#if QT_CONFIG(process)
    enum ProcessState {
        NotRunning = QProcess::NotRunning,
        Running = QProcess::Running,
        Starting = QProcess::Starting
    };
#else
    enum ProcessState {
        NotRunning,
        Running,
        Starting
    };
#endif
    Q_ENUM(ProcessState)

    cwTestcaseManager();


    Q_INVOKABLE void run();
    Q_INVOKABLE void stop();

    QString arguments() const;
    void setArguments(QString arguments);

    void clearOutput();

    QString result() const;

    ProcessState processState() const;

private:
    void addToResult(QString line);
    QStringList argumentList() const;

    QString Arguments; //!<
    QString Result; //!<

#if QT_CONFIG(process)
    QProcess* TestcaseProcess;
#endif

private slots:
#if QT_CONFIG(process)
    void handleError(QProcess::ProcessError error);
#endif

signals:
    void resultChanged();
    void resultCleared();
    void lineAdded(QString newLine);
    void argumentsChanged();
    void processStateChanged();
};

/**
* @brief cwTestcaseManager::arguments
* @return
*/
inline QString cwTestcaseManager::arguments() const {
    return Arguments;
}

/**
* @brief cwTestcaseManager::result
* @return
*/
inline QString cwTestcaseManager::result() const {
    return Result;
}

/**
* @brief class::processState
* @return
*/
inline cwTestcaseManager::ProcessState cwTestcaseManager::processState() const {
#if QT_CONFIG(process)
    return (ProcessState)TestcaseProcess->state();
#else
    return NotRunning;
#endif
}

#endif // CWTESTCASEMANAGER_H
