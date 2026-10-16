add_library(daemon_pairing STATIC src/PairStore.cpp src/PairStore.h)
target_include_directories(daemon_pairing PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(daemon_pairing PUBLIC daemon_core Qt6::Sql)

add_library(daemon_broker STATIC src/SessionBroker.cpp src/SessionBroker.h)
target_link_libraries(daemon_broker PUBLIC daemon_pairing Qt6::WebSockets)

add_executable(daemon-session-server src/main.cpp)
target_link_libraries(daemon-session-server PRIVATE daemon_broker)
