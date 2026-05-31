cmake_minimum_required(VERSION 3.19)
project(LuminaPrayer LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Multimedia OpenGL Concurrent Network)

qt_standard_project_setup()

qt_add_executable(LuminaPrayer
    WIN32 MACOSX_BUNDLE
    main.cpp
    widget.cpp
    widget.h
    roleact.h
    configmanager.cpp
    configmanager.h
    spriteresource.cpp
    spriteresource.h
    platformhal.cpp
    platformhal.h
    dragfilter.cpp
    dragfilter.h
    fistwidget.cpp
    fistwidget.h
    settingsdialog.cpp
    settingsdialog.h
    profilemanager.cpp
    profilemanager.h
    playmate.cpp
    playmate.h
    audiomanager.cpp
    audiomanager.h
    hardwaremanager.cpp
    hardwaremanager.h
    effectmanager.cpp
    effectmanager.h
    gomokuengine.cpp
    gomokuengine.h
    gomokuwidget.cpp
    gomokuwidget.h
    statusmanager.cpp
    statusmanager.h
    statuspanel.cpp
    statuspanel.h
    deepseekchat.cpp
    deepseekchat.h
    chatbubblewidget.cpp
    chatbubblewidget.h
    foodmenuwidget.cpp
    foodmenuwidget.h
    headpatwidget.cpp
    headpatwidget.h
    starwidget.cpp
    starwidget.h
    fontutils.h
    imgs.qrc
    audio.qrc
    shaders.qrc
)

target_link_libraries(LuminaPrayer
    PRIVATE
        Qt6::Core
        Qt6::Widgets
        Qt6::Multimedia
        Qt6::OpenGL
        Qt6::Concurrent
        Qt6::Network
)

# 【关键修改】Windows 平台链接 user32 库
if (WIN32)
    target_link_libraries(LuminaPrayer PRIVATE user32)
endif()

# 【关键修改】MSVC 全链路 UTF-8 编码
# /utf-8 = /source-charset:utf-8 + /execution-charset:utf-8
# 防止中文 GBK 编码碰撞（"饱" GBK B1A5 → Big5 "飥"）
if (MSVC)
    target_compile_options(LuminaPrayer PRIVATE /utf-8)
endif()

include(GNUInstallDirs)
install(TARGETS LuminaPrayer
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
