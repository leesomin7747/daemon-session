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
