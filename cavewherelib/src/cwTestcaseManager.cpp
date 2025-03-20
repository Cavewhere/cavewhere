/**************************************************************************
**
**    Copyright (C) 2016 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwTestcaseManager.h"
#include "cwGlobals.h"

//Qt includes
#include <QApplication>
#include <QRegularExpression>
#include <QDebug>

cwTestcaseManager::cwTestcaseManager() :
    TestcaseProcess(new QProcess(this))
{
    connect(TestcaseProcess, &QProcess::readyReadStandardOutput,
            this, [this](){addToResult(TestcaseProcess->readAllStandardOutput());});
    connect(TestcaseProcess, &QProcess::readyReadStandardError,
            this, [this](){addToResult(TestcaseProcess->readAllStandardError());});

    connect(TestcaseProcess, &QProcess::stateChanged, this, &cwTestcaseManager::processStateChanged);
    connect(TestcaseProcess, SIGNAL(error(QProcess::ProcessError)), SLOT(handleError(QProcess::ProcessError)));

    connect(TestcaseProcess, static_cast<void(QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
            [this](int exitCode, QProcess::ExitStatus exitStatus)
    {
        QString typeOfFinish;
        switch(exitStatus) {
        case QProcess::NormalExit:
            typeOfFinish = "finished";
            break;
        case QProcess::CrashExit:
            typeOfFinish = "crashed";
            break;
        }

        addToResult(QString("cavewhere-test has %1 with an exit code of %2").arg(typeOfFinish).arg(exitCode));
    });
}

/**
 * Runs the testcase application
 */
void cwTestcaseManager::run()
{
    clearOutput();

    QStringList cavewhereTestNames;
    cavewhereTestNames.append("cavewhere-test");
    cavewhereTestNames.append("cavewhere-test.exe");

    QString testcasePath = cwGlobals::findExecutable(cavewhereTestNames);

    if(testcasePath.isEmpty()) {
        addToResult("Error - Can't find cavewhere-test");
        return;
    }

    TestcaseProcess->start(testcasePath, argumentList());
}

/**
 * Kills the testcase application
 */
void cwTestcaseManager::stop()
{
    TestcaseProcess->kill();
}

/**
* Sets the arguments for the cavewhere-test to be run
*/
void cwTestcaseManager::setArguments(QString arguments) {
    if(Arguments != arguments) {
        Arguments = arguments;
        emit argumentsChanged();
    }
}

/**
 * Clears all the text in the output of the testcase
 *
 * If the testcases haven't been run, this will be empty
 */
void cwTestcaseManager::clearOutput()
{
    Result = QString();
    emit resultChanged();
    emit resultCleared();
}

/**
 * Add new text to the result, this will emit resultChanged() and lineAdded()
 * signal
 */
void cwTestcaseManager::addToResult(QString line)
{
    Result.append(line);
    emit resultChanged();
    emit lineAdded(line);
}

/**
 * This splits arguments based on spaces and quotes
 */
QStringList cwTestcaseManager::argumentList() const
{
    QStringList list;

    QString arguments = this->arguments();

    QRegularExpression findNext("(?:(?!\\\\)\"(.+)(?!\\\\)\")|(\\S+)");
    Q_ASSERT(findNext.isValid());

    QRegularExpressionMatchIterator i = findNext.globalMatch(arguments);
    while (i.hasNext()) {
        QRegularExpressionMatch match = i.next();

        if (!match.captured(1).isEmpty()) {
            QString quotedString = match.captured(1);
            quotedString.remove('\\');
            list.append(quotedString);
        } else {
            list.append(match.captured(2));
        }
    }

    return list;
}

/**
 * This prints out errors from the process
 */
void cwTestcaseManager::handleError(QProcess::ProcessError error)
{
    switch(error) {
    case QProcess::FailedToStart: addToResult("cavewhere-test failed to start"); break;
    case QProcess::Crashed: addToResult("cavewhere-test has crashed"); break;
    case QProcess::Timedout: addToResult("cavewhere-test has timedout"); break;
    case QProcess::WriteError: addToResult("cavewhere-test has had a write error"); break;
    case QProcess::ReadError: addToResult("cavewhere-test has had a read error"); break;
    case QProcess::UnknownError: addToResult("cavewhere-test has had an unknown error ... sorry :("); break;
    }
}

