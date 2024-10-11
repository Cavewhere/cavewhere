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

/**
 * @brief The cwTestcaseManager class
 *
 * This class runs cavewhere-test (test case application) and allow the cavewhere-test
 * output to be displayed in qml inside the application.
 */
class cwTestcaseManager : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString result READ result NOTIFY resultChanged)
    Q_PROPERTY(QString arguments READ arguments WRITE setArguments NOTIFY argumentsChanged)
    Q_PROPERTY(ProcessState processState READ processState NOTIFY processStateChanged)

    Q_ENUMS(ProcessState)
public:
    enum ProcessState {
        NotRunning = QProcess::NotRunning,
        Running = QProcess::Running,
        Starting = QProcess::Starting
    };

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
    QProcess* TestcaseProcess;

private slots:
    void handleError(QProcess::ProcessError error);

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
    return (ProcessState)TestcaseProcess->state();
}

#endif // CWTESTCASEMANAGER_H
