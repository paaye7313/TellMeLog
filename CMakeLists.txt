cmake_minimum_required(VERSION 3.19)
project(TellMeLog LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets LinguistTools)

qt_standard_project_setup()

qt_add_executable(TellMeLog
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    logparser.h
    logparser.cpp
)

qt_add_translations(
    TARGETS TellMeLog
    TS_FILES TellMeLog_ko_KR.ts
)

target_link_libraries(TellMeLog
    PRIVATE
        Qt::Core
        Qt::Widgets
)

install(TARGETS TellMeLog
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET TellMeLog
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
