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
#include "cwSettings.h"
#include "cwTask.h"
#include "cwMetaTypeSystem.h"

int main( int argc, char* argv[] )
{
  QApplication app(argc, argv);

  QApplication::setOrganizationName("Vadose Solutions");
  QApplication::setOrganizationDomain("cavewhere.com");
  QApplication::setApplicationName("cavewhere-test");
  QApplication::setApplicationVersion("1.0");

  cwMetaTypeSystem::registerTypes();

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

  return result;
}

