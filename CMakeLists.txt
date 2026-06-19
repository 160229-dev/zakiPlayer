cmake_minimum_required(VERSION 3.19)
project(zakiPlayer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Multimedia
    MultimediaWidgets
)

qt_standard_project_setup()

qt_add_executable(zakiPlayer
    WIN32 MACOSX_BUNDLE
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.h
    src/playercontroller.cpp
    src/playercontroller.h
    src/playlistmodel.cpp
    src/playlistmodel.h
    src/visualizer.cpp
    src/visualizer.h
    src/audiospectrum.cpp
    src/audiospectrum.h
    src/settingsmanager.cpp
    src/settingsmanager.h
    src/playlistwidget.cpp
    src/playlistwidget.h
    src/lyricsview.cpp
    src/lyricsview.h
    src/pinnedlyrics.cpp
    src/pinnedlyrics.h
    src/fullscreenlyrics.cpp
    src/fullscreenlyrics.h
    src/toast.cpp
    src/toast.h
    resources/resources.qrc
)

if(WIN32)
    target_sources(zakiPlayer PRIVATE app.rc)
endif()

target_link_libraries(zakiPlayer
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Multimedia
        Qt6::MultimediaWidgets
)

target_include_directories(zakiPlayer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

set_target_properties(zakiPlayer PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_NAME "zakiPlayer"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.zakiplayer.app"
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
)

install(TARGETS zakiPlayer
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET zakiPlayer
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
