qt_add_executable(overlay_probe main.cpp)
target_link_libraries(overlay_probe PRIVATE daemon_overlay)
qt_add_resources(overlay_probe probe_qml PREFIX "/probe" FILES Probe.qml Workbench.qml)
set_target_properties(overlay_probe PROPERTIES MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_GUI_IDENTIFIER "dev.daemonsession.overlay-probe"
    MACOSX_BUNDLE_BUNDLE_NAME "Daemon Overlay Probe")
