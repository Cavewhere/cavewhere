/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#define CATCH_CONFIG_RUNNER
#include "catch.hpp"

//Qt includes
#include <QApplication>
#include <QThread>

int main( int argc, char* argv[] )
{
  QApplication app(argc, argv);

  QApplication::setOrganizationName("Vadose Solutions");
  QApplication::setOrganizationDomain("cavewhere.com");
  QApplication::setApplicationName("cavewhere-test");
  QApplication::setApplicationVersion("1.0");

  app.thread()->setObjectName("Main QThread");

  int result = Catch::Session().run( argc, argv );

  return result;
}

