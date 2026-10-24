cmake_minimum_required(VERSION 3.24)

# Set the deployment target before project() enables the compiler and vcpkg.
if(CMAKE_HOST_APPLE AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "26.0" CACHE STRING "Minimum macOS version" FORCE)
endif()
# The arm64-osx overlay also builds native dependencies for macOS 26.0.
list(PREPEND VCPKG_OVERLAY_TRIPLETS "${CMAKE_CURRENT_SOURCE_DIR}/cmake/triplets")
list(REMOVE_DUPLICATES VCPKG_OVERLAY_TRIPLETS)
project(LSLViewer VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.9 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Test)
find_package(LSL CONFIG REQUIRED)
find_package(pugixml CONFIG REQUIRED)

# The vcpkg liblsl 1.17.7 static target omits its external loguru dependency.
# Restore it on the imported target so every consumer gets the required library.
get_target_property(LSL_LIBRARY_TYPE LSL::lsl TYPE)
if(LSL_LIBRARY_TYPE STREQUAL "STATIC_LIBRARY")
    find_package(loguru CONFIG REQUIRED)
    target_link_libraries(LSL::lsl INTERFACE loguru::loguru)
endif()

qt_standard_project_setup(REQUIRES 6.9)

if(MSVC)
    add_compile_options(/utf-8)
endif()

# ---------------------------------------------------------------------------
# Core library: no entry point. Shared by the GUI app, lslviewer_probe and the
# tests so that every piece of behaviour can be exercised without a window.
# ---------------------------------------------------------------------------
qt_add_library(lslviewer_core STATIC
    src/metadata.h
    src/metadata.cpp
    src/models.h
    src/models.cpp
    src/viewercontroller.h
    src/viewercontroller.cpp
)
target_include_directories(lslviewer_core PUBLIC src)
target_link_libraries(lslviewer_core
    PUBLIC Qt6::Core Qt6::Gui LSL::lsl pugixml::pugixml
)

# The UI lives in qml/ next to this file. It is embedded as a plain Qt resource
# instead of a QML module: qmlcachegen cannot handle files outside the source
# tree, and mirroring the file into the build tree breaks whenever the build
# directory moves.
set(SHARED_QML_DIR "${CMAKE_CURRENT_SOURCE_DIR}/qml")

# ---------------------------------------------------------------------------
# Product: GUI executable.
# ---------------------------------------------------------------------------
qt_add_executable(lslviewer src/app/main.cpp)
target_link_libraries(lslviewer PRIVATE lslviewer_core Qt6::Quick Qt6::QuickControls2)
set_target_properties(lslviewer PROPERTIES WIN32_EXECUTABLE ON)

if(APPLE)
    # macdeployqt scans qml/ explicitly below. Qt's default source-root scan
    # would also traverse staged DMGs and their Applications symlink.
    get_target_property(QT_CORE_LIBRARY_TYPE Qt6::Core TYPE)
    if(QT_CORE_LIBRARY_TYPE STREQUAL "SHARED_LIBRARY")
        set_target_properties(lslviewer PROPERTIES QT_QML_MODULE_NO_IMPORT_SCAN TRUE)
    endif()
    set_target_properties(lslviewer PROPERTIES
        MACOSX_BUNDLE TRUE
        OUTPUT_NAME "LSL Viewer"
        MACOSX_BUNDLE_BUNDLE_NAME "LSL Viewer"
        MACOSX_BUNDLE_GUI_IDENTIFIER "org.lslviewer.app"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Info.plist.in"
    )
endif()

qt_add_resources(lslviewer "shared_qml"
    PREFIX "/qml"
    BASE "${SHARED_QML_DIR}"
    FILES "${SHARED_QML_DIR}/Main.qml"
)

if(WIN32 AND TARGET Qt6::windeployqt)
    add_custom_command(TARGET lslviewer POST_BUILD
        COMMAND Qt6::windeployqt --no-translations --qmldir "${SHARED_QML_DIR}" "$<TARGET_FILE:lslviewer>"
        COMMENT "Deploying Qt runtime"
        VERBATIM
    )
endif()

# ---------------------------------------------------------------------------
# Probe: console executable that drives the same core and dumps one stream as
# JSON, so end-to-end tests never need GUI automation.
# ---------------------------------------------------------------------------
add_executable(lslviewer_probe src/app/probe_main.cpp)
target_link_libraries(lslviewer_probe PRIVATE lslviewer_core)

# ---------------------------------------------------------------------------
# Tests: C++ unit tests for the parser and a contract test for the shared QML.
# ---------------------------------------------------------------------------
enable_testing()

add_executable(tst_metadata tests/tst_metadata.cpp)
target_link_libraries(tst_metadata PRIVATE lslviewer_core Qt6::Test)
target_compile_definitions(tst_metadata PRIVATE
    FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
add_test(NAME metadata COMMAND tst_metadata)

add_executable(tst_qmlcontract tests/tst_qmlcontract.cpp)
target_link_libraries(tst_qmlcontract PRIVATE
    lslviewer_core Qt6::Quick Qt6::QuickControls2 Qt6::Test)
add_test(NAME qml_contract COMMAND tst_qmlcontract "${SHARED_QML_DIR}/Main.qml")

# Both test executables run from the build directory; point them at the Qt
# runtime, platform plugins and QML imports so they work without a Developer
# Prompt. The tests need the offscreen platform plugin, which windeployqt does
# not deploy for the GUI executable, so the Qt SDK plugin directory is used.
set_tests_properties(metadata qml_contract PROPERTIES
    ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;QML2_IMPORT_PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Qml>/../qml;QT_PLUGIN_PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>/../plugins"
)

if(APPLE)
    # Deploy into the install staging directory, leaving the development bundle
    # untouched. The QML is a plain resource, so scan its source imports explicitly.
    get_target_property(QT_QMAKE_EXECUTABLE Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(QT_BIN_DIR "${QT_QMAKE_EXECUTABLE}" DIRECTORY)
    find_program(MACDEPLOYQT_EXECUTABLE macdeployqt HINTS "${QT_BIN_DIR}" NO_DEFAULT_PATH REQUIRED)
    install(TARGETS lslviewer BUNDLE DESTINATION . COMPONENT Runtime)
    install(FILES LICENSE DESTINATION "LSL Viewer.app/Contents/Resources" COMPONENT Runtime)
    configure_file(cmake/DeployMacOS.cmake.in "${CMAKE_CURRENT_BINARY_DIR}/DeployMacOS.cmake" @ONLY)
    install(SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/DeployMacOS.cmake" COMPONENT Runtime)

    set(CPACK_GENERATOR DragNDrop)
    set(CPACK_PACKAGE_NAME "LSLViewer")
    set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
    set(CPACK_PACKAGE_VENDOR "LSLViewer")
    set(CPACK_PACKAGE_FILE_NAME "LSLViewer-${PROJECT_VERSION}-macOS${CMAKE_OSX_DEPLOYMENT_TARGET}-${CMAKE_SYSTEM_PROCESSOR}")
    set(CPACK_DMG_VOLUME_NAME "LSL Viewer")
    set(CPACK_DMG_FORMAT UDZO)
    set(CPACK_PACKAGING_INSTALL_PREFIX "/")
    set(CPACK_COMPONENTS_ALL Runtime)
    include(CPack)
endif()
