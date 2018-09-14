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

int main( int argc, char* argv[] )
{
  QApplication app(argc, argv);

  int result = Catch::Session().run( argc, argv );

  return result;
}

