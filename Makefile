.DEFAULT_GOAL := all

.PHONY: all release debug clean install package rpm deploy \
	test test-release test-debug coverage-report \
	check-valgrind check-coverage-tools entr

ARCH ?= $(shell rpm --eval '%{_target_cpu}' 2>/dev/null || uname -m)
VERSION ?= *DEVELOPMENT SNAPSHOT*
CXX := $(shell command -v clang++)

WITH_POSTGRES ?= 1
WITH_POSTGRES_TESTS ?= $(WITH_POSTGRES)
WITH_VALGRIND ?= 1
WITH_COVERAGE ?= 1
WITH_PROCESS_TESTS ?= 1
WITH_NETWORK_TESTS ?= 1
WITH_TIMING_TESTS ?= 1

CPPFLAGS ?=
CXXFLAGS ?=
LDFLAGS ?=
EXEFLAGS ?=
TEST_GTEST_FLAGS ?=

RELEASE_CXXFLAGS ?= -O3 -g -DNDEBUG -flto
DEBUG_CXXFLAGS ?= -O0 -g3 -fno-omit-frame-pointer
TEST_CXXFLAGS ?= -O0 -g3 -fno-omit-frame-pointer
COVERAGE_CXXFLAGS ?= -fprofile-instr-generate -fcoverage-mapping

ifeq ($(ARCH),x86_64)
	RELEASE_CXXFLAGS += -march=x86-64-v2
endif

PROJECT_CPPFLAGS :=
PACKAGE_NAMES := krb5 zlib

ifeq ($(WITH_POSTGRES),1)
	PROJECT_CPPFLAGS += -DEL1_WITH_POSTGRES
	PACKAGE_NAMES += libpq
endif

ifeq ($(WITH_VALGRIND),1)
	PROJECT_CPPFLAGS += -DEL1_WITH_VALGRIND
endif

TEST_EXCLUDE_PATTERNS :=

ifeq ($(WITH_POSTGRES_TESTS),0)
	TEST_EXCLUDE_PATTERNS += db_postgres.*
endif

ifeq ($(WITH_PROCESS_TESTS),0)
	TEST_EXCLUDE_PATTERNS += system_task.TProcess_* io_net_http.THttpServer_*
endif

ifeq ($(WITH_NETWORK_TESTS),0)
	TEST_EXCLUDE_PATTERNS += io_net_ip.ResolveHostname
endif

ifeq ($(WITH_TIMING_TESTS),0)
	TEST_EXCLUDE_PATTERNS += system_time.TTime_Now
endif

empty :=
space := $(empty) $(empty)
ifneq ($(strip $(TEST_EXCLUDE_PATTERNS)),)
	TEST_GTEST_FLAGS += '--gtest_filter=-$(subst $(space),:,$(strip $(TEST_EXCLUDE_PATTERNS)))'
endif

ifeq ($(WITH_COVERAGE),1)
	DEBUG_COVERAGE_CXXFLAGS := $(COVERAGE_CXXFLAGS)
else
	DEBUG_COVERAGE_CXXFLAGS :=
endif

COMMON_CXXFLAGS := -Wall -Wextra -Wno-unused-command-line-argument \
	-Wno-unused-parameter -Wno-unused-const-variable -Wno-vla-extension \
	-Wno-deprecated-declarations -std=c++20 $(CXXFLAGS)
COMMON_LDFLAGS := -fuse-ld=lld $(LDFLAGS)
PACKAGE_CXXFLAGS := $(shell pkg-config --cflags $(PACKAGE_NAMES))
LIB_CXXFLAGS := -fPIC $(PACKAGE_CXXFLAGS)
EXE_CXXFLAGS := -fPIE
LIB_LDFLAGS := $(shell pkg-config --libs $(PACKAGE_NAMES)) -Wl,--no-undefined
GTEST_CXXFLAGS := -Wno-unused-command-line-argument -fuse-ld=lld -fPIC $(CXXFLAGS) $(TEST_CXXFLAGS)

OUT_DIR ?= gen
RELEASE_DIR := $(OUT_DIR)/release
DEBUG_DIR := $(OUT_DIR)/debug
GTEST_DIR := $(OUT_DIR)/gtest
COVERAGE_DIR := $(DEBUG_DIR)/coverage
COVERAGE_PROFILE_DIR := $(COVERAGE_DIR)/profiles
COVERAGE_HTML_DIR := $(COVERAGE_DIR)/html
COVERAGE_DATA := $(COVERAGE_DIR)/coverage.profdata
TEST_WORK_DIRS ?= $(OUT_DIR)/test.tmp $(OUT_DIR)/test1.tmp

LIB_DIR ?= /usr/lib
INCLUDE_DIR ?= /usr/include
KEYID ?= BE5096C665CA4595AF11DAB010CD9FF74E4565ED
ARCH_RPM_NAME := el1.$(ARCH).rpm
SRC_RPM_NAME := el1.src.rpm
SPEC_NAME := el1.spec

RELEASE_LIB_NAME := $(RELEASE_DIR)/libel1.so
DEBUG_LIB_NAME := $(DEBUG_DIR)/libel1.so
RELEASE_TEST_NAME := $(RELEASE_DIR)/gtest.exe
DEBUG_TEST_NAME := $(DEBUG_DIR)/gtest.exe
SUPER_HEADER := $(OUT_DIR)/el1.hpp
GTEST_LIB := $(GTEST_DIR)/lib/libgtest.a
GTEST_MAIN_LIB := $(GTEST_DIR)/lib/libgtest_main.a

LIB_SOURCES := $(wildcard src/el1/*.cpp)
LIB_HEADERS := $(wildcard src/el1/*.hpp)
TEST_SOURCES := $(wildcard src/el1/test/*.cpp)
TEST_HEADERS := $(wildcard src/el1/test/*.hpp)

RELEASE_LIB_OBJECTS := $(patsubst src/el1/%.cpp,$(RELEASE_DIR)/obj/%.o,$(LIB_SOURCES))
DEBUG_LIB_OBJECTS := $(patsubst src/el1/%.cpp,$(DEBUG_DIR)/obj/%.o,$(LIB_SOURCES))
RELEASE_TEST_OBJECTS := $(patsubst src/el1/test/%.cpp,$(RELEASE_DIR)/test/%.o,$(TEST_SOURCES))
DEBUG_TEST_OBJECTS := $(patsubst src/el1/test/%.cpp,$(DEBUG_DIR)/test/%.o,$(TEST_SOURCES))
DEP_FILES := \
	$(RELEASE_LIB_OBJECTS:.o=.d) \
	$(DEBUG_LIB_OBJECTS:.o=.d) \
	$(RELEASE_TEST_OBJECTS:.o=.d) \
	$(DEBUG_TEST_OBJECTS:.o=.d)

VALGRIND ?= valgrind
VALGRIND_FLAGS ?= \
	--quiet \
	--leak-check=full \
	--show-reachable=no \
	--track-origins=yes \
	--num-callers=30 \
	--trace-children=no \
	--error-exitcode=1 \
	--suppressions=support/valgrind.sup \
	--gen-suppressions=all
LLVM_PROFDATA ?= llvm-profdata
LLVM_COV ?= llvm-cov

ifeq ($(WITH_VALGRIND),1)
	TEST_RUNNER := $(VALGRIND) $(VALGRIND_FLAGS)
else
	TEST_RUNNER :=
endif

export CXX

all: release debug

release: $(RELEASE_LIB_NAME) $(SUPER_HEADER)

debug: $(DEBUG_LIB_NAME) $(SUPER_HEADER)

clean:
	rm -rf -- "$(OUT_DIR)" *.rpm

$(SUPER_HEADER): $(LIB_HEADERS)
	@mkdir -p "$(@D)"
	echo "#pragma once" > "$@.tmp"
	for header in $(LIB_HEADERS); do echo "#include \"$${header#src/el1/*}\""; done >> "$@.tmp"
	mv -f -- "$@.tmp" "$@"

$(RELEASE_DIR)/obj/%.o: src/el1/%.cpp Makefile
	@mkdir -p "$(@D)"
	$(CXX) $(CPPFLAGS) $(PROJECT_CPPFLAGS) $(COMMON_CXXFLAGS) $(RELEASE_CXXFLAGS) $(LIB_CXXFLAGS) -MMD -MP -c -o "$@" "$<"

$(DEBUG_DIR)/obj/%.o: src/el1/%.cpp Makefile
	@mkdir -p "$(@D)"
	$(CXX) $(CPPFLAGS) $(PROJECT_CPPFLAGS) $(COMMON_CXXFLAGS) $(DEBUG_CXXFLAGS) $(DEBUG_COVERAGE_CXXFLAGS) $(LIB_CXXFLAGS) -MMD -MP -c -o "$@" "$<"

$(RELEASE_LIB_NAME): $(RELEASE_LIB_OBJECTS) $(LIB_HEADERS)
	@mkdir -p "$(@D)"
	$(CXX) $(COMMON_LDFLAGS) $(RELEASE_CXXFLAGS) -shared -o "$@" $(RELEASE_LIB_OBJECTS) $(LIB_LDFLAGS)

$(DEBUG_LIB_NAME): $(DEBUG_LIB_OBJECTS) $(LIB_HEADERS)
	@mkdir -p "$(@D)"
	$(CXX) $(COMMON_LDFLAGS) $(DEBUG_CXXFLAGS) $(DEBUG_COVERAGE_CXXFLAGS) -shared -o "$@" $(DEBUG_LIB_OBJECTS) $(LIB_LDFLAGS)

$(RELEASE_DIR)/test/%.o: src/el1/test/%.cpp Makefile
	@mkdir -p "$(@D)"
	$(CXX) $(CPPFLAGS) $(PROJECT_CPPFLAGS) $(COMMON_CXXFLAGS) $(TEST_CXXFLAGS) $(EXE_CXXFLAGS) $(PACKAGE_CXXFLAGS) \
		"-DVERSION=\"$(VERSION)\"" -I submodules/googletest/googletest/include -I src -MMD -MP -c -o "$@" "$<"

$(DEBUG_DIR)/test/%.o: src/el1/test/%.cpp Makefile
	@mkdir -p "$(@D)"
	$(CXX) $(CPPFLAGS) $(PROJECT_CPPFLAGS) $(COMMON_CXXFLAGS) $(TEST_CXXFLAGS) $(DEBUG_COVERAGE_CXXFLAGS) $(EXE_CXXFLAGS) $(PACKAGE_CXXFLAGS) \
		"-DVERSION=\"$(VERSION)\"" -I submodules/googletest/googletest/include -I src -MMD -MP -c -o "$@" "$<"

$(GTEST_LIB): Makefile
	@mkdir -p "$(GTEST_DIR)"
	(P="$$PWD"; cd "$(GTEST_DIR)" && \
		cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="$(GTEST_CXXFLAGS)" "$$P/submodules/googletest" && \
		$(MAKE))

$(GTEST_MAIN_LIB): $(GTEST_LIB)
	@test -f "$@"

$(RELEASE_TEST_NAME): $(RELEASE_TEST_OBJECTS) $(GTEST_LIB) $(GTEST_MAIN_LIB) $(RELEASE_LIB_NAME) $(SUPER_HEADER)
	@mkdir -p "$(@D)"
	$(CXX) $(COMMON_LDFLAGS) $(TEST_CXXFLAGS) $(EXE_CXXFLAGS) -o "$@" \
		$(RELEASE_TEST_OBJECTS) $(GTEST_LIB) $(GTEST_MAIN_LIB) -L"$(RELEASE_DIR)" -lel1 $(LIB_LDFLAGS) $(EXEFLAGS)

$(DEBUG_TEST_NAME): $(DEBUG_TEST_OBJECTS) $(GTEST_LIB) $(GTEST_MAIN_LIB) $(DEBUG_LIB_NAME) $(SUPER_HEADER)
	@mkdir -p "$(@D)"
	$(CXX) $(COMMON_LDFLAGS) $(TEST_CXXFLAGS) $(DEBUG_COVERAGE_CXXFLAGS) $(EXE_CXXFLAGS) -o "$@" \
		$(DEBUG_LIB_OBJECTS) $(DEBUG_TEST_OBJECTS) $(GTEST_LIB) $(GTEST_MAIN_LIB) $(LIB_LDFLAGS) $(EXEFLAGS)

ifeq ($(WITH_VALGRIND),1)
check-valgrind:
	command -v "$(VALGRIND)" >/dev/null
else
check-valgrind:
	@:
endif

ifeq ($(WITH_COVERAGE),1)
check-coverage-tools:
	command -v "$(LLVM_PROFDATA)" >/dev/null
	command -v "$(LLVM_COV)" >/dev/null
else
check-coverage-tools:
	@:
endif

test:
	$(MAKE) --no-print-directory test-release
	$(MAKE) --no-print-directory coverage-report

test-release: check-valgrind $(RELEASE_TEST_NAME)
	./support/generate-testdata.sh
	rm -rf -- $(TEST_WORK_DIRS) && mkdir -p -- $(TEST_WORK_DIRS)
	LD_LIBRARY_PATH="$(RELEASE_DIR)" $(TEST_RUNNER) "$(RELEASE_TEST_NAME)" $(TEST_GTEST_FLAGS)

test-debug: check-valgrind $(DEBUG_TEST_NAME)
	./support/generate-testdata.sh
	rm -rf -- $(TEST_WORK_DIRS) "$(COVERAGE_PROFILE_DIR)" && mkdir -p -- $(TEST_WORK_DIRS)
	mkdir -p "$(COVERAGE_PROFILE_DIR)"
	LLVM_PROFILE_FILE="$(abspath $(COVERAGE_PROFILE_DIR))/%m-%p.profraw" \
		LD_LIBRARY_PATH="$(DEBUG_DIR)" \
		$(TEST_RUNNER) "$(DEBUG_TEST_NAME)" $(TEST_GTEST_FLAGS)

ifeq ($(WITH_COVERAGE),1)
coverage-report: check-coverage-tools test-debug
	$(LLVM_PROFDATA) merge -sparse "$(COVERAGE_PROFILE_DIR)"/*.profraw -o "$(COVERAGE_DATA)"
	rm -rf -- "$(COVERAGE_HTML_DIR)"
	$(LLVM_COV) show "$(DEBUG_TEST_NAME)" \
		-instr-profile="$(COVERAGE_DATA)" \
		-format=html \
		-output-dir="$(COVERAGE_HTML_DIR)" \
		-show-line-counts-or-regions \
		-show-instantiations=false \
		--sources $(LIB_SOURCES) $(LIB_HEADERS)
	@echo "Coverage report: file://$(abspath $(COVERAGE_HTML_DIR))/index.html"
else
coverage-report: test-debug
	@echo "Coverage disabled (WITH_COVERAGE=0)"
endif

$(ARCH_RPM_NAME) $(SRC_RPM_NAME): $(LIB_SOURCES) $(LIB_HEADERS) $(SPEC_NAME) Makefile
	easy-rpm.sh --debug --name el1 --spec "$(SPEC_NAME)" --outdir . --plain --arch "$(ARCH)" -- $^

install: release
	rm -rf -- "$(INCLUDE_DIR)/el1"
	mkdir -p "$(LIB_DIR)" "$(INCLUDE_DIR)/el1"
	install -m 755 "$(RELEASE_LIB_NAME)" "$(LIB_DIR)/"
	install -m 644 $(LIB_HEADERS) "$(SUPER_HEADER)" "$(INCLUDE_DIR)/el1/"

package: rpm

rpm: $(ARCH_RPM_NAME)

deploy: $(ARCH_RPM_NAME)
	ensure-git-clean.sh
	deploy-rpm.sh --infile="$(SRC_RPM_NAME)" --outdir="$(RPMDIR)" --keyid="$(KEYID)"
	deploy-rpm.sh --infile="$(ARCH_RPM_NAME)" --outdir="$(RPMDIR)" --keyid="$(KEYID)"

entr: $(LIB_HEADERS) $(LIB_SOURCES) $(TEST_SOURCES) $(TEST_HEADERS)
	printf '%s\n' $^ | entr bash -c 'clear; reset; make test'

-include $(DEP_FILES)
