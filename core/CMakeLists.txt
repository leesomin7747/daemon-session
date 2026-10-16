add_library(daemon_core STATIC Protocol.cpp Protocol.h Session.cpp Session.h CopyCatalog.cpp CopyCatalog.h WireProtocol.cpp WireProtocol.h)
target_include_directories(daemon_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(daemon_core PUBLIC Qt6::Core)
qt_add_resources(daemon_core copy PREFIX "/daemon" FILES copy/ko.json)
