add_library(daemon_scene STATIC src/SceneModel.cpp src/SceneModel.h)
target_include_directories(daemon_scene PUBLIC src)
target_link_libraries(daemon_scene PUBLIC daemon_core Qt6::Quick Qt6::QuickControls2)

qt_add_executable(daemon-session MACOSX_BUNDLE src/main.cpp)
target_link_libraries(daemon-session PRIVATE daemon_scene daemon_overlay)
qt_add_resources(daemon-session scene PREFIX "/desktop" BASE qml FILES
    qml/Main.qml qml/Pet.qml qml/Prop.qml qml/ActionMenu.qml)
