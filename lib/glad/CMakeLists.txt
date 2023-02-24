cmake_minimum_required(VERSION 3.14)
project(glad)


if (APPLE)
	add_library(glad darwin/glad/glad.h darwin/KHR/khrplatform.h darwin/glad.c)
	target_include_directories(glad PUBLIC darwin/)
else()
	add_library(glad banana/glad/glad.h banana/KHR/khrplatform.h banana/glad.c)
	target_include_directories(glad PUBLIC banana/)
endif()
