# Generated by CMake, DO NOT EDIT

TARGETS:= 
empty:= 
space:= $(empty) $(empty)
spaceplus:= $(empty)\ $(empty)

TARGETS += $(subst $(space),$(spaceplus),$(wildcard /Applications/CMake.app/Contents/share/cmake-3.25/Modules/FindPackageHandleStandardArgs.cmake))
TARGETS += $(subst $(space),$(spaceplus),$(wildcard /Applications/CMake.app/Contents/share/cmake-3.25/Modules/FindPackageMessage.cmake))
TARGETS += $(subst $(space),$(spaceplus),$(wildcard /Applications/CMake.app/Contents/share/cmake-3.25/Modules/FindPkgConfig.cmake))
TARGETS += $(subst $(space),$(spaceplus),$(wildcard /Users/justus/dev/vibes/lib/FFmpeg/CMakeLists.txt))

/Users/justus/dev/vibes/build/CMakeFiles/cmake.check_cache: $(TARGETS)
	/Applications/CMake.app/Contents/bin/cmake -H/Users/justus/dev/vibes -B/Users/justus/dev/vibes/build
