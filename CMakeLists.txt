cmake_minimum_required(VERSION 3.16)

project(CaveWhereSketch VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Catch2 REQUIRED)
find_package(Qt6
    REQUIRED COMPONENTS
    Quick
    QuickShapesPrivate
)

qt_standard_project_setup(REQUIRES 6.5)

qt_add_executable(CaveWhereSketch
    main.cpp
    testcases/test_PainterPathModel.cpp
)

qt_add_qml_module(cavewhere_sketch
    URI CaveWhereSketch
    VERSION 1.0
    QML_FILES
        Main.qml
        SOURCES PenLineModel.h PenLineModel.cpp
        SOURCES PenLineView.h PenLineView.cpp
        SOURCES PainterPathModel.h PainterPathModel.cpp
        SOURCES SketchShapePath.h SketchShapePath.cpp
)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
set_target_properties(CaveWhereSketch PROPERTIES
#    MACOSX_BUNDLE_GUI_IDENTIFIER com.vadosesolutions.CaveWhereSketch
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(cavewhere_sketch
    PUBLIC
    Qt6::Quick
    PRIVATE
    Qt6::QuickShapesPrivate
)

target_include_directories(cavewhere_sketch
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(CaveWhereSketch
    PRIVATE
    cavewhere_sketch
)

target_compile_definitions(cavewhere_sketch
    PRIVATE QT_USE_PRIVATE_HEADERS
)

# include(GNUInstallDirs)
# install(TARGETS CaveWhereSketch
#     BUNDLE DESTINATION .
#     LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
#     RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
# )


qt_add_executable(cavewhere-sketch-test)

set_target_properties(cavewhere-sketch-test PROPERTIES
    AUTOMOC ON
    AUTORCC ON
    AUTOUIC ON
)

file(GLOB cavewhere_sketch_test_CPP_FILES "testcases/*.cpp")
file(GLOB cavewhere_sketch_test_H_FILES "testcases/*.h")
target_sources(cavewhere-sketch-test PRIVATE ${cavewhere_sketch_test_CPP_FILES} ${cavewhere_sketch_test_H_FILES})

target_link_libraries(cavewhere-sketch-test
    PRIVATE
    Catch2::Catch2
    cavewhere_sketch
)


