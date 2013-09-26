INCLUDEPATH += ../lib/QMath3d
LIBS += -L ../lib/QMath3d

CONFIG(debug, debug|release) {
    LIBS += -lQMath3d_debug
}

CONFIG(release, debug|release) {
    LIBS += -lQMath3d
}
