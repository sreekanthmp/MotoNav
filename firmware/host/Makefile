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
#   make sim-tools         build the simulators that run hw_check.cpp and oled_check.cpp
#                          against a simulated board (see tests_py/)
#   make test-tools        sim-tools + Python tests for the log analyser and the simulators
#
# These results are HOST results. They say nothing about the physical display.

CXX      ?= g++
# Override with SANITIZE= (empty) if the compiler has no sanitizer runtime.
SANITIZE ?= -fsanitize=address,undefined -fno-omit-frame-pointer
LIBSRC   := ../lib/motonav/src
LIBPROTO := ../lib/motonav_protocol/src
LIBCPP   := $(wildcard $(LIBSRC)/core/*.cpp $(LIBSRC)/ui/*.cpp $(LIBSRC)/sim/*.cpp $(LIBSRC)/diag/*.cpp \
                        $(LIBPROTO)/protocol/*.cpp)
TESTCPP  := $(wildcard tests/*.cpp)
CXXFLAGS := -std=c++11 -O1 -g -Wall -Wextra -Wpedantic -Werror $(SANITIZE) \
            -I$(LIBSRC) -I$(LIBPROTO) -I.

.PHONY: test render golden-update check-layering sim-tools test-tools reference clean

# Simulated-board builds: the REAL firmware/src/tools sources, compiled on the
# host against the shims in shim/. -D values mirror platformio.ini.
SIMFLAGS := -std=c++11 -O1 -g -Wall -Wextra -Wpedantic -Werror $(SANITIZE) \
            -Ishim -I../src -I$(LIBSRC) -I. \
            -DMOTONAV_OLED_WIDTH=128 -DMOTONAV_OLED_HEIGHT=64 -DMOTONAV_OLED_I2C_ADDR=0x3C \
            -DMOTONAV_I2C_SDA=21 -DMOTONAV_I2C_SCL=22
SIMDEPS  := shim/shim.cpp $(wildcard shim/*.h) $(wildcard $(LIBSRC)/*/*.h)

build/hw_check_sim: hw_check_sim.cpp ../src/tools/hw_check.cpp $(LIBSRC)/diag/hw_verdict.cpp $(SIMDEPS)
	@mkdir -p build
	$(CXX) $(SIMFLAGS) hw_check_sim.cpp ../src/tools/hw_check.cpp $(LIBSRC)/diag/hw_verdict.cpp shim/shim.cpp -o $@

build/oled_check_sim: oled_check_sim.cpp ../src/tools/oled_check.cpp font5x7.h $(SIMDEPS)
	@mkdir -p build
	$(CXX) $(SIMFLAGS) oled_check_sim.cpp ../src/tools/oled_check.cpp shim/shim.cpp -o $@

sim-tools: build/hw_check_sim build/oled_check_sim

build/stats_sample: stats_sample.cpp $(LIBSRC)/diag/log_format.cpp $(LIBSRC)/diag/frame_check.cpp $(wildcard $(LIBSRC)/diag/*.h) $(wildcard $(LIBPROTO)/protocol/*)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) stats_sample.cpp $(LIBSRC)/diag/log_format.cpp $(LIBSRC)/diag/frame_check.cpp $(LIBPROTO)/protocol/ble_log.cpp $(LIBPROTO)/protocol/receiver.cpp $(LIBPROTO)/protocol/frame.cpp $(LIBPROTO)/protocol/crc16.cpp $(LIBSRC)/core/nav_model.cpp -o $@

# Replays frames through the real Receiver and prints the serial lines the firmware would print.
build/ble_script: ble_script.cpp $(wildcard $(LIBPROTO)/protocol/*) $(LIBSRC)/ui/view.cpp
	@mkdir -p build
	$(CXX) $(CXXFLAGS) ble_script.cpp $(LIBPROTO)/protocol/ble_log.cpp $(LIBPROTO)/protocol/receiver.cpp $(LIBPROTO)/protocol/frame.cpp $(LIBPROTO)/protocol/crc16.cpp $(LIBSRC)/ui/view.cpp $(LIBSRC)/core/nav_model.cpp $(LIBSRC)/core/distance_format.cpp -o $@

# Feeds frames through the firmware's real codec (see tests_py/test_frame_tool.py).
build/frame_check: frame_check.cpp $(wildcard $(LIBPROTO)/protocol/*)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) frame_check.cpp $(LIBPROTO)/protocol/frame.cpp $(LIBPROTO)/protocol/crc16.cpp -o $@

test-tools: sim-tools build/stats_sample build/frame_check build/ble_script
	python3 -I -m unittest discover -s tests_py -v

test: check-layering build/run_tests
	./build/run_tests

build/run_tests: $(LIBCPP) $(TESTCPP) $(wildcard *.h) $(wildcard $(LIBSRC)/*/*.h) $(wildcard $(LIBPROTO)/*/*.h)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $(LIBCPP) $(TESTCPP) -o $@

build/render_screens: $(LIBCPP) render_screens.cpp $(wildcard *.h) $(wildcard $(LIBSRC)/*/*.h) $(wildcard $(LIBPROTO)/*/*.h)
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

# Regenerates the reference material in ../../docs/reference used by TESTING.md:
# what each screen / test pattern should look like, and the exact serial text of
# the hw_check scenarios and the T4 walkthrough. All HOST-generated predictions,
# not photographs of real hardware.
REF := ../../docs/reference
reference: build/render_screens build/oled_check_sim build/hw_check_sim build/t4_script
	rm -f out/*.pbm out/*.png
	./build/render_screens out
	./build/oled_check_sim out > /dev/null
	python3 -I montage.py out
	mkdir -p $(REF)
	cp out/montage_128x64.png $(REF)/screens_128x64.png
	cp out/montage_t4dist.png $(REF)/t4_distances.png
	cp out/montage_t4road.png $(REF)/t4_road_names.png
	cp out/montage_oled.png $(REF)/oled_check_patterns.png
	./build/t4_script > $(REF)/t4_expected_serial.txt
	( for s in healthy oled_at_3d both_addresses other_device no_oled swapped_wires sda_short no_pullups \
	           intermittent slow_only wrong_chip_s3 no_ble flash_2mb flash_8mb flash_unreadable single_core; do \
	    echo "################ simulated scenario: $$s"; ./build/hw_check_sim $$s 2; echo; \
	  done ) > $(REF)/hw_check_simulated_outputs.txt

build/t4_script: t4_script.cpp $(LIBSRC)/sim/sim.cpp $(LIBSRC)/ui/view.cpp $(LIBSRC)/core/*.cpp $(wildcard $(LIBSRC)/*/*.h)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) t4_script.cpp $(LIBSRC)/sim/sim.cpp $(LIBSRC)/ui/view.cpp $(LIBSRC)/core/distance_format.cpp $(LIBSRC)/core/nav_model.cpp -o $@
