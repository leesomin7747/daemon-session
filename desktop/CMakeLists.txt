add_library(daemon_scene STATIC src/SceneModel.cpp src/SceneModel.h)
target_include_directories(daemon_scene PUBLIC src)
target_link_libraries(daemon_scene PUBLIC daemon_core Qt6::Quick Qt6::QuickControls2)

add_library(daemon_tray STATIC src/DesktopTray.cpp src/DesktopTray.h)
target_include_directories(daemon_tray PUBLIC src)
target_link_libraries(daemon_tray PUBLIC Qt6::Widgets)

qt_add_executable(daemon-session MACOSX_BUNDLE src/main.cpp)
target_link_libraries(daemon-session PRIVATE daemon_scene daemon_overlay daemon_tray)
qt_add_resources(daemon-session scene PREFIX "/desktop" BASE qml FILES
    qml/Main.qml qml/Pet.qml qml/Prop.qml qml/ActionMenu.qml
    qml/PanelFrame.qml qml/PanelButton.qml qml/MemoEditor.qml qml/MemoField.qml
    qml/WipPanel.qml qml/NoteComposer.qml qml/InboxPanel.qml)
