# Host-side build: unit tests and screen rendering. No PlatformIO and no
# hardware needed; only a C++11 compiler and make.
#
#   make test              build with sanitizers, run layering check and all tests
#   make -B test CXX=clang++ SANITIZE=   second compiler, no sanitizers
#                          (leave SANITIZE out if your clang ships the runtimes)
#   make render            write every screen to out/*.pbm (and out/montage.png
#                          if Python with Pillow is available)
#   make golden-update     accept the current rendering as the new golden set
#
# These results are HOST results. They say nothing about the physical display.

CXX      ?= g++
# Override with SANITIZE= (empty) if the compiler has no sanitizer runtime.
SANITIZE ?= -fsanitize=address,undefined -fno-omit-frame-pointer
LIBSRC   := ../lib/motonav/src
LIBCPP   := $(wildcard $(LIBSRC)/core/*.cpp $(LIBSRC)/ui/*.cpp $(LIBSRC)/sim/*.cpp)
TESTCPP  := $(wildcard tests/*.cpp)
CXXFLAGS := -std=c++11 -O1 -g -Wall -Wextra -Wpedantic -Werror $(SANITIZE) \
            -I$(LIBSRC) -I.

.PHONY: test render golden-update check-layering clean

test: check-layering build/run_tests
	./build/run_tests

build/run_tests: $(LIBCPP) $(TESTCPP) $(wildcard *.h) $(wildcard $(LIBSRC)/*/*.h)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $(LIBCPP) $(TESTCPP) -o $@

build/render_screens: $(LIBCPP) render_screens.cpp $(wildcard *.h) $(wildcard $(LIBSRC)/*/*.h)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $(LIBCPP) render_screens.cpp -o $@

render: build/render_screens
	./build/render_screens out
	-python3 -I montage.py out

golden-update: build/run_tests
	GOLDEN_UPDATE=1 ./build/run_tests golden

check-layering:
	sh check_layering.sh

clean:
	rm -rf build out
