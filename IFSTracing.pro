TEMPLATE = app
CONFIG += console c++14
CONFIG -= app_bundle
CONFIG -= qt

DEFINES += GLEW_STATIC

LIBS += -lglfw3dll -lglew32s -lopengl32

include(ge1/ge1.pri)

SOURCES += \
    fractal.cpp \
    main.cpp \
    $$files(imgui/*.cpp, true)

DISTFILES += \
    trace.glsl

HEADERS += \
    fractal.h \
    $$files(imgui/*.h, true)
