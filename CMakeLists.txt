cmake_minimum_required(VERSION 3.14)
project(vibes C CXX)
set(CMAKE_CXX_STANDARD 20)

add_subdirectory(lib/glfw)
add_subdirectory(lib/FFmpeg)
add_subdirectory(lib/glad)


if (APPLE)
	list (APPEND EXTRA_LIBS
		"-framework OpenGL"
	)
	
	list(APPEND SOURCES lib/glad/darwin/glad.c)

	configure_file(
        	${CMAKE_CURRENT_SOURCE_DIR}/assets/MacOSXBundleInfo.plist.in
        	${CMAKE_CURRENT_BINARY_DIR}/assets/MacOSXBundleInfo.plist
    	)
else()
	list (APPEND EXTRA_LIBS
		"-lGL -ldl -lX11 -pthread -lavcodec -lavformat -lswscale -lavutil -lavfilter -lswresample -lm -lz"
		)

	list(APPEND SOURCES lib/glad/banana/glad.c)
endif()

list(APPEND SOURCES
	src/main.cpp
	src/video_reader.cpp
	src/video_reader.hpp
	src/shader.h
	src/tic_tac_toe_vertices.h
	src/frame_producer.cpp
	src/frame_producer.hpp
	src/shaders/rgb
	src/shaders/texture
	src/network.cpp
	src/network.h
	src/synchronization_handler.cpp
	src/synchronization_handler.hpp
	)

add_executable(vibes ${SOURCES})

if (APPLE)
	set_target_properties(vibes PROPERTIES
		MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_BINARY_DIR}/assets/MacOSXBundleInfo.plist
	)
endif()
 
target_link_libraries(vibes FFmpeg ${EXTRA_LIBS} glad glfw)
