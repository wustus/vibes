cmake_minimum_required(VERSION 3.14)
project(FFmpeg)

find_package(PkgConfig REQUIRED)

pkg_check_modules(AVCODEC	 REQUIRED IMPORTED_TARGET libavcodec)
pkg_check_modules(AVFORMAT	 REQUIRED IMPORTED_TARGET libavformat)
pkg_check_modules(AVUTIL	 REQUIRED IMPORTED_TARGET libavutil)
pkg_check_modules(SWSCALE	 REQUIRED IMPORTED_TARGET libswscale)

add_library(FFmpeg INTERFACE IMPORTED GLOBAL)
target_include_directories(FFmpeg INTERFACE
	${AV_CODEC_INCLUDE_DIRS}
	${AV_FORMAT_INCLUDE_DIRS}
	${AV_UTIL_INCLUDE_DIRS}
	${SWSCALE_INCLUDE_DIRS}
)

target_link_options(FFmpeg INTERFACE
	${AVCODEC_LDFLAGS}
	${AVFORMAT_LDFLAGS}
	${AVUTIL_LDFLAGS}
	${SWSCALE_LDFLAGS}
)
