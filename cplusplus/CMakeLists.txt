cmake_minimum_required(VERSION 3.24)
project(LSLViewer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.9 REQUIRED COMPONENTS Gui Quick QuickControls2)
find_package(LSL CONFIG REQUIRED)
find_package(pugixml CONFIG REQUIRED)

qt_standard_project_setup(REQUIRES 6.9)

qt_add_executable(lslviewer
    src/main.cpp
    src/metadata.h
    src/metadata.cpp
    src/models.h
    src/models.cpp
    src/viewercontroller.h
    src/viewercontroller.cpp
)

# The UI lives in ../qml and is shared with the Python project. qmlcachegen cannot handle
# files outside the source tree, so mirror it into the build tree (re-copied on change).
set(SHARED_QML_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../qml")
configure_file("${SHARED_QML_DIR}/Main.qml" "${CMAKE_CURRENT_BINARY_DIR}/Main.qml" COPYONLY)
set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/Main.qml"
    PROPERTIES QT_RESOURCE_ALIAS Main.qml)

qt_add_qml_module(lslviewer
    URI LslViewer
    VERSION 1.0
    QML_FILES "${CMAKE_CURRENT_BINARY_DIR}/Main.qml"
)

if(MSVC)
    target_compile_options(lslviewer PRIVATE /utf-8)
endif()

set_target_properties(lslviewer PROPERTIES WIN32_EXECUTABLE ON)

target_include_directories(lslviewer PRIVATE src)
target_link_libraries(lslviewer PRIVATE
    Qt6::Gui
    Qt6::Quick
    Qt6::QuickControls2
    LSL::lsl
    pugixml::pugixml
)

if(WIN32 AND TARGET Qt6::windeployqt)
    add_custom_command(TARGET lslviewer POST_BUILD
        COMMAND Qt6::windeployqt --no-translations --qmldir "${SHARED_QML_DIR}" "$<TARGET_FILE:lslviewer>"
        COMMENT "Deploying Qt runtime"
        VERBATIM
    )
endif()