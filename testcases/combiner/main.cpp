
//Qt includes
#include <QProcess>
#include <QCoreApplication>
#include <QDebug>
#include <QTimer>
#include <QRegularExpression>

//AsyncFuture
#include "asyncfuture.h"

//Std includes
#include <unordered_set>
#include <algorithm>

int main( int argc, char* argv[] )
{

    QCoreApplication app(argc, argv);

    QString execName = "cavewhere-test.exe";
    int timeout = 1000 * 15;
    QString testArg = "--list-tests";
    QString newline = "\r\n";

    auto testCaseNames = [execName, testArg, newline]()->QStringList {
        QProcess process;
        process.setProgram(execName);
        process.setArguments({testArg});
        process.start();
        bool okay = process.waitForFinished(10000);

        if(!okay) {
            qDebug() << "Failed to finish looking for testcases" << process.errorString();
        }

        auto output = process.readAllStandardOutput();
        auto outputStr = QString::fromLocal8Bit(output);

        QStringList testcases = outputStr.split(newline);

        testcases.removeFirst();

        //Remove all the extra spaces
        std::transform(testcases.begin(), testcases.end(), testcases.begin(),
                       [](const QString& str)->QString
        {
            return str.trimmed();
        });

        auto removeIter = std::remove_if(testcases.begin(), testcases.end(),
                                       [](const QString& str)
        {
            return str.isEmpty();
        });
        testcases.erase(removeIter, testcases.end());

        return testcases;
    };

    auto prettyPrint = [newline](const QString& str) {
        auto lines = str.split(newline);
        QDebug debug = qDebug();
        debug.noquote();
        for(auto line : lines) {
            auto trimmed = line.trimmed();
            if(!trimmed.isEmpty()) {
                debug << "\t\t" << trimmed << "\n";
            }
        }
    };

    auto runWithArg = [execName, timeout, prettyPrint](QStringList args) {

        auto defer = std::make_shared<AsyncFuture::Deferred<bool>>(AsyncFuture::deferred<bool>());

        auto process = new QProcess();
        auto timer = new QTimer(process);

        QObject::connect(timer, &QTimer::timeout,
                         timer, [process]()
        {
            qDebug() << "Process timed out!" << process->arguments();
            process->kill();
        });

        QObject::connect(process, &QProcess::readyReadStandardError,
                         process, [timer, timeout, prettyPrint, process]()
        {
            prettyPrint(process->readAllStandardOutput());
            timer->start(timeout);
        });

        QObject::connect(process, &QProcess::readyReadStandardOutput,
                         process, [timer, timeout, prettyPrint, process]()
        {
            prettyPrint(process->readAllStandardOutput());
            timer->start(timeout);
        });

        QObject::connect(process, static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
                         process,
                         [process, args, defer, prettyPrint](int exitCode, QProcess::ExitStatus exitStatus)
        {
            if(exitCode == 0 && exitStatus == QProcess::NormalExit) {
                qDebug() << "\tSuccess" << process->arguments();
                defer->complete(true);
            } else {
                qDebug() << "Process failed" << process->arguments();
                defer->complete(false);
            }

        });

        process->setProgram(execName);
        process->setArguments(QStringList({"-d", "yes"}) + args);
        qDebug() << "\tStarting" << process->arguments();
        process->start();
        timer->start(timeout);

        AsyncFuture::observe(defer->future())
                .subscribe([process]()
        {
            delete process;
        });

        return defer->future();
    };

    auto filterTags = [](QStringList testcases) {
        QRegularExpression regExp("^\\[.+\\]$");


        std::unordered_set<QString> set;
        auto end = std::copy_if(testcases.begin(),
                                testcases.end(),
                                testcases.begin(),
                                [regExp, &set](const QString& str)
        {
            auto match = regExp.match(str);
            if(match.hasMatch()) {
                return set.insert(str).second;
            }
            return false;
        });

        testcases.erase(end, testcases.end());
        return testcases;
    };


    auto runTestCases = [runWithArg](QStringList testcases)->QFuture<int> {
        class Context {
        public:
            AsyncFuture::Deferred<int> defer;
            int iter = 0;
            std::function<void ()> runNextTextcase;
        };

        auto context = new Context();
        context->defer = AsyncFuture::deferred<int>();
        context->runNextTextcase =
                [context,
                testcases,
                runWithArg]()
        {
            if(context->iter < testcases.size()) {
                auto future = runWithArg({testcases.at(context->iter)});
                AsyncFuture::observe(future).subscribe(
                            [future,
                            context]()
                {
                    auto success = future.result();
                    if(!success) {
                        qDebug() << "Failed!";
                        context->defer.complete(-1);
                        return;
                    }
                    context->iter++;
                    context->runNextTextcase();
                });
            } else {
                context->defer.complete(0);
                return;
            }
        };

        context->runNextTextcase();

        AsyncFuture::observe(context->defer.future()).subscribe(
                    [context]()
        {
            delete context;
        });

        return context->defer.future();
    };

    auto testcases = filterTags(testCaseNames());

//    auto failingTestCase = "[CavewhereMainWindow],[ProtoSaveLoad],[cwAddImageTask],[cwCropImageTask],[cwDXT1Compresser],[cwFutureFilterModel],[cwFutureManagerModel],[LinePlotManager],[cwOpenGLSettings],[cwProject],[cwScrap],[cwTextureUploadTask]";
//    auto failingTestCase = "[CavewhereMainWindow],[ProtoSaveLoad],[cwAddImageTask],[cwCropImageTask],[cwDXT1Compresser],[cwFutureFilterModel],[cwFutureManagerModel],[LinePlotManager],[cwOpenGLSettings],[cwProject],[cwScrap],[cwTextureUploadTask]";

//    auto failingTestCase = "[ErrorModelTest],[Error],[FindUnconnectedSurveyChunksTask],[SurveyChunkSignaler],[SurveyNetwork],[cwTripCalibration],[cwWallsImporter],[CavewhereMainWindow],[Compass],[ProtoSaveLoad],[cwAddImageTask],[cwCSVImporterManager],[cwCSVLineModel],[cwCropImageTask],[cwDXT1Compresser],[cwFutureFilterModel],[cwFutureManagerModel],[cwImage],[cwJob],[cwJobSettings],[cwLead],[LinePlotManager],[cwOpenGLSettings],[cwProject],[cwRegionIOTask],[cwRegionTreeModel],[cwScrap],[cwSettings],[SurvexImport],[cwSurveyChunk],[cwSurveyNetwork],[cwTextureUploadTask],[version],[dewalls],[dewalls, azimuth],[dewalls, inclination],[dewalls, length],[WallsProjectParser],[WallsProjectParser, .PATH]";
   auto failingTestCase = "[cwJobSettings],[cwScrap]";

    auto future = runTestCases({failingTestCase});
//    runTestCases({testcases.join(",")});

//    //Minimize the number of testcase that fail
//    int iter = 0;
//    std::function<void ()> expand =
//            [
//            &expand,
//            testcases,
//            failingTestCase,
//            &iter,
//            runTestCases
//            ]() {
//        if(iter > testcases.size()) {
//            QCoreApplication::exit(0);
//        }
//        QStringList newTestcase(testcases.begin(), testcases.begin() + iter);
//        QString run = newTestcase.join(",") + "," + failingTestCase;
//        auto future = runTestCases({run});
//        AsyncFuture::observe(future).subscribe([future, &iter, &expand, run](){
//            if(future.result() == 0) {
//                iter++;
//                expand();
//            } else {
//                //Found first bad
//                qDebug() << "First bad:" << run;
//                QCoreApplication::exit(future.result());
//            }
//        });
//    };

//    expand();

//    QStringList specificTests;
//    std::transform(testcases.begin(), testcases.end(),
//                   std::back_inserter(specificTests),
//                   [](const QString& testcase)
//    {
//        return testcase + ",[cwProject]";
//    });

//    auto future = runTestCases(specificTests); //testcases);

    AsyncFuture::observe(future).subscribe([future](){
        QCoreApplication::exit(future.result());
    });

    app.exec();

}
