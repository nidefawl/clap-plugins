set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)

# GUI interfaces
add_library(
  clap-plugin-gui-common
  abstract-gui.hh abstract-gui.cc abstract-gui-listener.hh
  abstract-gui-listener.cc abstract-gui-factory.hh abstract-gui-factory.cc
  gui-handle.hh gui-handle.cc)
set_property(TARGET clap-plugin-gui-common PROPERTY CXX_STANDARD 20)
set_property(TARGET clap-plugin-gui-common PROPERTY POSITION_INDEPENDENT_CODE
                                                    true)
target_link_libraries(clap-plugin-gui-common PUBLIC clap-core)

# Actual GUI implementation and display
add_library(
  clap-plugin-gui
  gui.hh
  gui.cc
  parameter-proxy.hh
  parameter-proxy.cc
  plugin-proxy.hh
  plugin-proxy.cc
  transport-proxy.hh
  transport-proxy.cc)
set_property(TARGET clap-plugin-gui PROPERTY CXX_STANDARD 20)
target_link_libraries(clap-plugin-gui PUBLIC clap-io clap-plugin-gui-common)
target_link_libraries(clap-plugin-gui PUBLIC Qt6::Quick)
set_property(TARGET clap-plugin-gui PROPERTY POSITION_INDEPENDENT_CODE true)
qt6_import_qml_plugins(clap-plugin-gui)

if(CLAP_PLUGINS_EMBED_QML)
  add_subdirectory(qml)
  target_link_libraries(clap-plugin-gui PUBLIC clap-qml-skins clap-qml-lib clap-qml-libplugin)
endif()

if(CLAP_PLUGINS_REMOTE_GUI)
  # Code for having the GUI in a child process
  add_library(
    clap-plugin-remote-gui
    remote-gui-factory-proxy.hh remote-gui-factory-proxy.cc remote-gui-proxy.hh
    remote-gui-proxy.cc)
  set_property(TARGET clap-plugin-remote-gui PROPERTY CXX_STANDARD 20)
  set_property(TARGET clap-plugin-remote-gui PROPERTY POSITION_INDEPENDENT_CODE true)
  target_link_libraries(clap-plugin-remote-gui PUBLIC clap-plugin-gui-common clap-io)
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(clap-plugin-remote-gui PUBLIC pthread)
  endif()

  add_executable(
    clap-gui gui-main.cc remote-gui-factory.hh remote-gui-factory.cc
             remote-gui-listener.hh remote-gui-listener.cc)
  target_link_libraries(clap-gui PUBLIC clap-plugin-gui)
  set_target_properties(clap-gui PROPERTIES CXX_STANDARD 20)

  install(TARGETS clap-gui DESTINATION "bin")
else()
  # Code for having the GUI with the plugin
  add_library(
    clap-plugin-local-gui
    local-gui-factory.hh
    local-gui-factory.cc
    threaded-gui-factory.hh
    threaded-gui-factory.cc
    threaded-gui-proxy.hh
    threaded-gui-proxy.cc
    timer.hh
    timer.cc
    cf-timer.hh
    cf-timer.cc
    win32-timer.hh
    win32-timer.cc)
  set_property(TARGET clap-plugin-local-gui PROPERTY CXX_STANDARD 20)
  set_property(TARGET clap-plugin-local-gui PROPERTY POSITION_INDEPENDENT_CODE true)
  target_link_libraries(clap-plugin-local-gui PUBLIC clap-plugin-gui)
endif()
