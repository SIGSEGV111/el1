.DEFAULT_GOAL := all

.PHONY: all release debug clean install package rpm deploy \
	test test-release test-debug coverage-report \
	check-valgrind check-coverage-tools entr

ARCH ?= $(shell rpm --eval '%{_target_cpu}' 2>/dev/null || uname -m)
VERSION ?= *DEVELOPMENT SNAPSHOT*
CXX := $(shell command -v clang++)

CPPFLAGS ?=
CXXFLAGS ?=
LDFLAGS ?=
EXEFLAGS ?=

RELEASE_CXXFLAGS ?= -O3 -g -DNDEBUG -flto
DEBUG_CXXFLAGS ?= -O0 -g3 -fno-omit-frame-pointer
TEST_CXXFLAGS ?= -O0 -g3 -fno-omit-frame-pointer
COVERAGE_CXXFLAGS ?= -fprofile-instr-generate -fcoverage-mapping

ifeq ($(ARCH),x86_64)
	RELEASE_CXXFLAGS += -march=x86-64-v2
endif

PROJECT_CPPFLAGS := -DEL1_WITH_POSTGRES -DEL1_WITH_VALGRIND
COMMON_CXXFLAGS := -Wall -Wextra -Wno-unused-command-line-argument \
	-Wno-unused-parameter -Wno-unused-const-variable -Wno-vla-extension \
	-Wno-deprecated-declarations -std=c++20 $(CXXFLAGS)
COMMON_LDFLAGS := -fuse-ld=lld $(LDFLAGS)
PACKAGE_CXXFLAGS := $(shell pkg-config --cflags libpq krb5 zlib)
LIB_CXXFLAGS := -fPIC $(PACKAGE_CXXFLAGS)
EXE_CXXFLAGS := -fPIE
LIB_LDFLAGS := $(shell pkg-config --libs libpq krb5 zlib) -Wl,--no-undefined
GTEST_CXXFLAGS := -Wno-unused-command-line-argument -fuse-ld=lld -fPIC $(CXXFLAGS) $(TEST_CXXFLAGS)

OUT_DIR ?= gen
RELEASE_DIR := $(OUT_DIR)/release
DEBUG_DIR := $(OUT_DIR)/debug
TEST_DIR := $(OUT_DIR)/test
GTEST_DIR := $(OUT_DIR)/gtest
COVERAGE_DIR := $(DEBUG_DIR)/coverage
COVERAGE_PROFILE_DIR := $(COVERAGE_DIR)/profiles
COVERAGE_HTML_DIR := $(COVERAGE_DIR)/html
COVERAGE_DATA := $(COVERAGE_DIR)/coverage.profdata

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
TEST_OBJECTS := $(patsubst src/el1/test/%.cpp,$(TEST_DIR)/%.o,$(TEST_SOURCES))
DEP_FILES := \
	$(RELEASE_LIB_OBJECTS:.o=.d) \
	$(DEBUG_LIB_OBJECTS:.o=.d) \
	$(TEST_OBJECTS:.o=.d)

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
	$(CXX) $(CPPFLAGS) $(PROJECT_CPPFLAGS) $(COMMON_CXXFLAGS) $(DEBUG_CXXFLAGS) $(COVERAGE_CXXFLAGS) $(LIB_CXXFLAGS) -MMD -MP -c -o "$@" "$<"

$(RELEASE_LIB_NAME): $(RELEASE_LIB_OBJECTS) $(LIB_HEADERS)
	@mkdir -p "$(@D)"
	$(CXX) $(COMMON_LDFLAGS) $(RELEASE_CXXFLAGS) -shared -o "$@" $(RELEASE_LIB_OBJECTS) $(LIB_LDFLAGS)

$(DEBUG_LIB_NAME): $(DEBUG_LIB_OBJECTS) $(LIB_HEADERS)
	@mkdir -p "$(@D)"
	$(CXX) $(COMMON_LDFLAGS) $(DEBUG_CXXFLAGS) $(COVERAGE_CXXFLAGS) -shared -o "$@" $(DEBUG_LIB_OBJECTS) $(LIB_LDFLAGS)

$(TEST_DIR)/%.o: src/el1/test/%.cpp Makefile
	@mkdir -p "$(@D)"
	$(CXX) $(CPPFLAGS) $(PROJECT_CPPFLAGS) $(COMMON_CXXFLAGS) $(TEST_CXXFLAGS) $(EXE_CXXFLAGS) $(PACKAGE_CXXFLAGS) \
		"-DVERSION=\"$(VERSION)\"" -I submodules/googletest/googletest/include -I src -MMD -MP -c -o "$@" "$<"

$(GTEST_LIB): Makefile
	@mkdir -p "$(GTEST_DIR)"
	(P="$$PWD"; cd "$(GTEST_DIR)" && \
		cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="$(GTEST_CXXFLAGS)" "$$P/submodules/googletest" && \
		$(MAKE))

$(GTEST_MAIN_LIB): $(GTEST_LIB)
	@test -f "$@"

$(RELEASE_TEST_NAME): $(TEST_OBJECTS) $(GTEST_LIB) $(GTEST_MAIN_LIB) $(RELEASE_LIB_NAME) $(SUPER_HEADER)
	@mkdir -p "$(@D)"
	$(CXX) $(COMMON_LDFLAGS) $(TEST_CXXFLAGS) $(EXE_CXXFLAGS) -o "$@" \
		$(TEST_OBJECTS) $(GTEST_LIB) $(GTEST_MAIN_LIB) -L"$(RELEASE_DIR)" -lel1 $(EXEFLAGS)

$(DEBUG_TEST_NAME): $(TEST_OBJECTS) $(GTEST_LIB) $(GTEST_MAIN_LIB) $(DEBUG_LIB_NAME) $(SUPER_HEADER)
	@mkdir -p "$(@D)"
	$(CXX) $(COMMON_LDFLAGS) $(TEST_CXXFLAGS) $(COVERAGE_CXXFLAGS) $(EXE_CXXFLAGS) -o "$@" \
		$(TEST_OBJECTS) $(GTEST_LIB) $(GTEST_MAIN_LIB) -L"$(DEBUG_DIR)" -lel1 $(EXEFLAGS)

check-valgrind:
	command -v "$(VALGRIND)" >/dev/null

check-coverage-tools: check-valgrind
	command -v "$(LLVM_PROFDATA)" >/dev/null
	command -v "$(LLVM_COV)" >/dev/null

test:
	$(MAKE) --no-print-directory test-release
	$(MAKE) --no-print-directory coverage-report

test-release: check-valgrind $(RELEASE_TEST_NAME)
	./support/generate-testdata.sh
	rm -rf -- "$(OUT_DIR)/test.tmp" && mkdir -- "$(OUT_DIR)/test.tmp"
	LD_LIBRARY_PATH="$(RELEASE_DIR)" $(VALGRIND) $(VALGRIND_FLAGS) "$(RELEASE_TEST_NAME)"

test-debug: check-coverage-tools $(DEBUG_TEST_NAME)
	./support/generate-testdata.sh
	rm -rf -- "$(OUT_DIR)/test.tmp" "$(COVERAGE_PROFILE_DIR)" && mkdir -- "$(OUT_DIR)/test.tmp"
	mkdir -p "$(COVERAGE_PROFILE_DIR)"
	LLVM_PROFILE_FILE="$(abspath $(COVERAGE_PROFILE_DIR))/%m-%p.profraw" \
		LD_LIBRARY_PATH="$(DEBUG_DIR)" \
		$(VALGRIND) $(VALGRIND_FLAGS) "$(DEBUG_TEST_NAME)"

coverage-report: test-debug
	$(LLVM_PROFDATA) merge -sparse "$(COVERAGE_PROFILE_DIR)"/*.profraw -o "$(COVERAGE_DATA)"
	rm -rf -- "$(COVERAGE_HTML_DIR)"
	$(LLVM_COV) show "$(DEBUG_LIB_NAME)" \
		-instr-profile="$(COVERAGE_DATA)" \
		-format=html \
		-output-dir="$(COVERAGE_HTML_DIR)" \
		-show-line-counts-or-regions \
		-show-instantiations=false \
		--sources $(LIB_SOURCES) $(LIB_HEADERS)
	@echo "Coverage report: file://$(abspath $(COVERAGE_HTML_DIR))/index.html"

$(ARCH_RPM_NAME) $(SRC_RPM_NAME): $(LIB_SOURCES) $(LIB_HEADERS) $(SPEC_NAME) Makefile
	easy-rpm.sh --debug --name el1 --spec "$(SPEC_NAME)" --outdir . --plain --arch "$(ARCH)" -- $^

install: release
	rm -rf -- "$(INCLUDE_DIR)/el1"
	mkdir -p "$(LIB_DIR)" "$(INCLUDE_DIR)/el1"
	install -m 755 "$(RELEASE_LIB_NAME)" "$(LIB_DIR)/"
	install -m 644 $(LIB_HEADERS) "$(SUPER_HEADER)" "$(INCLUDE_DIR)/el1/"

package: rpm

rpm: release $(ARCH_RPM_NAME)

deploy: release $(ARCH_RPM_NAME)
	ensure-git-clean.sh
	deploy-rpm.sh --infile="$(SRC_RPM_NAME)" --outdir="$(RPMDIR)" --keyid="$(KEYID)" --srpm
	deploy-rpm.sh --infile="$(ARCH_RPM_NAME)" --outdir="$(RPMDIR)" --keyid="$(KEYID)"

entr: $(LIB_HEADERS) $(LIB_SOURCES) $(TEST_SOURCES) $(TEST_HEADERS)
	printf '%s\n' $^ | entr bash -c 'clear; reset; make test'

-include $(DEP_FILES)
