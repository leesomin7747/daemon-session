add_executable(tst_overlaygeometry tst_overlaygeometry.cpp)
target_link_libraries(tst_overlaygeometry PRIVATE daemon_overlay Qt6::Test)
add_test(NAME overlaygeometry COMMAND tst_overlaygeometry)
set_tests_properties(overlaygeometry PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_test(NAME overlaygeometry_125 COMMAND tst_overlaygeometry)
set_tests_properties(overlaygeometry_125 PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_SCALE_FACTOR=1.25"
)

add_test(NAME overlay_probe_smoke COMMAND overlay_probe --smoke-test)
set_tests_properties(overlay_probe_smoke PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software"
    TIMEOUT 3
)

add_executable(tst_overlayqml tst_overlayqml.cpp)
target_link_libraries(tst_overlayqml PRIVATE daemon_overlay Qt6::Test)
target_compile_definitions(tst_overlayqml PRIVATE PROBE_QML_FILE="${PROJECT_SOURCE_DIR}/probes/overlay/Probe.qml")
add_test(NAME overlayqml COMMAND tst_overlayqml)
set_tests_properties(overlayqml PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")

foreach(name protocol session)
    add_executable(tst_${name} tst_${name}.cpp)
    target_link_libraries(tst_${name} PRIVATE daemon_core Qt6::Test)
    add_test(NAME ${name} COMMAND tst_${name})
endforeach()

add_executable(tst_pairing tst_pairing.cpp)
target_link_libraries(tst_pairing PRIVATE daemon_pairing Qt6::Test)
add_test(NAME pairing COMMAND tst_pairing)

add_executable(tst_broker tst_broker.cpp)
target_link_libraries(tst_broker PRIVATE daemon_broker Qt6::Test)
add_dependencies(tst_broker daemon-session-server)
add_test(NAME broker COMMAND tst_broker)
set_tests_properties(broker PROPERTIES TIMEOUT 45)

add_executable(tst_scenemodel tst_scenemodel.cpp)
target_link_libraries(tst_scenemodel PRIVATE daemon_scene Qt6::Test)
add_test(NAME pet_model COMMAND tst_scenemodel)
add_executable(tst_pet tst_pet.cpp)
target_link_libraries(tst_pet PRIVATE daemon_scene Qt6::QuickTest)
target_compile_definitions(tst_pet PRIVATE QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/qml")
add_test(NAME pet COMMAND tst_pet)
set_tests_properties(pet PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_QUICK_CONTROLS_STYLE=Basic" TIMEOUT 25)
add_executable(tst_petwindow tst_petwindow.cpp)
target_link_libraries(tst_petwindow PRIVATE daemon_scene daemon_overlay Qt6::Test)
target_compile_definitions(tst_petwindow PRIVATE PET_MAIN_FILE="${PROJECT_SOURCE_DIR}/desktop/qml/Main.qml")
add_dependencies(tst_petwindow daemon-session)
add_test(NAME pet_window COMMAND tst_petwindow)
add_test(NAME pet_window_125 COMMAND tst_petwindow)
set_tests_properties(pet_window PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_QUICK_CONTROLS_STYLE=Basic")
set_tests_properties(pet_window_125 PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_QUICK_CONTROLS_STYLE=Basic;QT_SCALE_FACTOR=1.25")
add_test(NAME pet_app_smoke COMMAND daemon-session --smoke-test)
set_tests_properties(pet_app_smoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software" TIMEOUT 3)
