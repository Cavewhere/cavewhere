/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#define CATCH_CONFIG_RUNNER
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_session.hpp>

//Qt includes
#include <QApplication>
#include <QThread>
#include <QThreadPool>
#include <QMetaObject>
#include <QThreadPool>
#include <QSettings>

//Our includes
#include "cwRootData.h"
#include "GitRepository.h"
#include "cwSettings.h"
#include "cwTask.h"
#include "cwMetaTypeSystem.h"
#include "cwGlobals.h"

#ifdef Q_OS_MACOS
#include <CoreFoundation/CoreFoundation.h>
#include <objc/message.h>
#include <objc/runtime.h>

namespace {
    //NSActivityUserInitiatedAllowingIdleSystemSleep, from NSProcessInfo.h
    constexpr unsigned long long kUserInitiatedActivity = 0x00FFFFFFULL & ~(1ULL << 20);

    //App Nap moves a windowless app into the background band within a minute
    //of launch: its threads leave the performance cores and its timers are
    //coalesced. A long run then crawls (a LiDAR texture encode goes from seconds
    //to minutes) and blows through test timeouts, while the same test run alone
    //finishes before the nap starts. Holding a user-initiated activity for the
    //whole run keeps the scheduler treating the runner as foreground work.
    void holdUserInitiatedActivity()
    {
        using ClassMessage = id (*)(Class, SEL);
        using BeginActivityMessage = id (*)(id, SEL, unsigned long long, id);
        using ObjectMessage = id (*)(id, SEL);

        const auto sendToClass = reinterpret_cast<ClassMessage>(objc_msgSend);
        const auto beginActivity = reinterpret_cast<BeginActivityMessage>(objc_msgSend);
        const auto sendToObject = reinterpret_cast<ObjectMessage>(objc_msgSend);

        id processInfo = sendToClass(objc_getClass("NSProcessInfo"), sel_registerName("processInfo"));
        id reason = reinterpret_cast<id>(const_cast<void*>(static_cast<const void*>(CFSTR("Running cavewhere-test"))));
        id activity = beginActivity(processInfo,
                                    sel_registerName("beginActivityWithOptions:reason:"),
                                    kUserInitiatedActivity,
                                    reason);

        //The token is autoreleased, and the activity ends once it is freed. It is
        //kept for the life of the process on purpose.
        sendToObject(activity, sel_registerName("retain"));
    }
}
#endif

int main( int argc, char* argv[] )
{
  QApplication app(argc, argv);

#ifdef Q_OS_MACOS
  holdUserInitiatedActivity();
#endif

  QApplication::setOrganizationName("Vadose Solutions");
  QApplication::setOrganizationDomain("cavewhere.com");
  QApplication::setApplicationName(QStringLiteral("cavewhere-test-%1").arg(QApplication::applicationPid()));
  QApplication::setApplicationVersion("1.0");

  cwMetaTypeSystem::registerTypes();

  //Load all the fonts
  cwGlobals::loadFonts();

  //initilize gitlib2
  cwRootData::initCavewherelib();
  // QQuickGit::GitRepository::initGitEngine();

  {
      QSettings settings;
      settings.clear();
  }

  cwSettings::initialize();

  app.thread()->setObjectName("Main QThread");

  int result = 0;
  QMetaObject::invokeMethod(&app, [&result, argc, argv]() {
      result = Catch::Session().run( argc, argv );
      QThreadPool::globalInstance()->waitForDone();
      cwTask::threadPool()->waitForDone();
      QApplication::quit();
  }, Qt::QueuedConnection);

  app.exec();

  QQuickGit::GitRepository::shutdownGitEngine();

  return result;
}

