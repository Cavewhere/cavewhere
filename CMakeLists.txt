cmake_minimum_required(VERSION 3.23)

project(CaveWhereSketch VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(QT_QML_GENERATE_QMLLS_INI ON CACHE BOOL "Generate QMLLS INI file")

# Include the GenerateExportHeader module
include(GenerateExportHeader)

# Include cavewhere
add_subdirectory(cavewhere/cavewherelib)
add_subdirectory(cavewhere/asyncfuture)
add_subdirectory(cavewhere/QMath3d)
add_subdirectory(cavewhere/monad)
add_subdirectory(cavewhere/QuickQanava)
add_subdirectory(cavewhere/dewalls)
add_subdirectory(cavewhere/MarkScope)

find_package(Catch2 REQUIRED)

# Normally, Catch2 is expected to be installed locally;
# if you'd rather not do that (e.g. for cross compilation
# or environment reasons) you may replace the find_package()
# call above with this, to download and build from source:
#
# include(FetchContent)
# FetchContent_Declare(
#     Catch2
#     GIT_REPOSITORY https://github.com/catchorg/Catch2.git
#     GIT_TAG 2b60af89e23d28eefc081bc930831ee9d45ea58b # v3.8.1
#     GIT_SHALLOW TRUE
# )
# FetchContent_MakeAvailable(Catch2)


find_package(Qt6
    REQUIRED COMPONENTS
    Core
    Quick
    QuickShapesPrivate
    Test
)

qt_standard_project_setup(REQUIRES 6.5)

qt_add_executable(CaveWhereSketch
    main.cpp
)

qt_add_qml_module(cavewhere_sketch
    URI CaveWhereSketch
    VERSION 1.0
    OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/CaveWhereSketch
    QML_FILES
        Main.qml
        SOURCES
        PenLineModel.h PenLineModel.cpp
        PenLineView.h PenLineView.cpp
        PainterPathModel.h PainterPathModel.cpp
        SketchShapePath.h SketchShapePath.cpp
        MovingAverageProxyModel.cpp MovingAverageProxyModel.h
        ExclusivePointHandler.h ExclusivePointHandler.cpp
        RootData.h RootData.cpp
        AbstractPainterPathModel.h AbstractPainterPathModel.cpp
        CenterlinePainterModel.h CenterlinePainterModel.cpp
        QML_FILES SketchPage.qml
        TripCompactPage.qml
        QML_FILES ShapePathInstantiator.qml
)

# Generate the export header for windows
generate_export_header(cavewhere_sketch
    BASE_NAME CAVEWHERE_SKETCH_LIB
    EXPORT_MACRO_NAME CAVEWHERE_SKETCH_LIB_EXPORT
    EXPORT_FILE_NAME CaveWhereSketchLibExport.h
)

# For including the CaveWhereSketchLibExport.h
target_include_directories(cavewhere_sketch PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}
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
    Qt6::Core
    cavewherelib
    PRIVATE
    Qt6::QuickPrivate
    Qt6::QuickShapesPrivate
)

target_include_directories(cavewhere_sketch
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE
    ${Qt6Quick_PRIVATE_INCLUDE_DIRS}
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


qt_add_executable(cavewhere-sketch-test
    testcases/test_CenterlinePainterModel.cpp)

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
    cavewhere_sketch
    Catch2::Catch2
    Qt6::Test
)


