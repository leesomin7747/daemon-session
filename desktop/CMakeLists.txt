add_library(daemon_scene STATIC src/SceneModel.cpp src/SceneModel.h)
target_include_directories(daemon_scene PUBLIC src)
target_link_libraries(daemon_scene PUBLIC daemon_core Qt6::Quick Qt6::QuickControls2)

find_package(Qt6Keychain REQUIRED)
add_library(daemon_client STATIC src/CredentialStore.cpp src/CredentialStore.h src/SessionClient.cpp src/SessionClient.h)
target_include_directories(daemon_client PUBLIC src)
target_link_libraries(daemon_client PUBLIC daemon_core Qt6::WebSockets Qt6Keychain::Qt6Keychain)

add_library(daemon_tray STATIC src/DesktopTray.cpp src/DesktopTray.h)
target_include_directories(daemon_tray PUBLIC src)
target_link_libraries(daemon_tray PUBLIC Qt6::Widgets)

qt_add_executable(daemon-session MACOSX_BUNDLE src/main.cpp)
set_target_properties(daemon-session PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER "io.github.leesomin7747.daemon-session"
    MACOSX_BUNDLE_BUNDLE_NAME "Daemon Session"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
)
target_link_libraries(daemon-session PRIVATE daemon_scene daemon_overlay daemon_tray daemon_client)
qt_add_resources(daemon-session scene PREFIX "/desktop" BASE qml FILES
    qml/Main.qml qml/Pet.qml qml/Prop.qml qml/ActionMenu.qml
    qml/PanelFrame.qml qml/PanelButton.qml qml/MemoEditor.qml qml/MemoField.qml
    qml/WipPanel.qml qml/NoteComposer.qml qml/InboxPanel.qml qml/ConnectPanel.qml)
