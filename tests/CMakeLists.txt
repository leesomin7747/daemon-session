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
