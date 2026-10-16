add_library(daemon_pairing STATIC src/PairStore.cpp src/PairStore.h)
target_include_directories(daemon_pairing PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(daemon_pairing PUBLIC daemon_core Qt6::Sql)
