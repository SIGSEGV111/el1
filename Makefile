.DEFAULT_GOAL := all

# -----------------------------------------------------------------------------
# Toolchain and user configuration
# -----------------------------------------------------------------------------

CXX := clang++
ARCH ?= $(shell rpm --eval '%{_target_cpu}' 2>/dev/null || uname -m)
EASY_RPM_VERSION ?= easy-rpm-version.sh
PACKAGE_VERSION ?= $(shell $(EASY_RPM_VERSION))
PACKAGE_VERSION := $(strip $(PACKAGE_VERSION))
ifeq ($(PACKAGE_VERSION),)
$(error unable to determine PACKAGE_VERSION; install easy-rpm or set PACKAGE_VERSION/RPM_VERSION explicitly)
endif
VERSION ?= Version $(PACKAGE_VERSION)

WITH_POSTGRES ?= 1
WITH_POSTGRES_TESTS ?= $(WITH_POSTGRES)
WITH_BLUETOOTH ?= 1
WITH_VALGRIND ?= 1
WITH_COVERAGE ?= 1
WITH_PROCESS_TESTS ?= 1
WITH_NETWORK_TESTS ?= 1
WITH_TIMING_TESTS ?= 1

CPPFLAGS ?=
CXXFLAGS ?=
LDFLAGS ?=
EXEFLAGS ?=
TEST_CXXFLAGS ?=
TEST_GTEST_FLAGS ?=
LIFETIME_EXTRA_CXXFLAGS ?=

RELEASE_CXXFLAGS ?= -O3 -g -DNDEBUG -flto
DEBUG_CXXFLAGS ?= -O0 -g3 -fno-omit-frame-pointer
GTEST_CXXFLAGS ?= -O0 -g3 -fno-omit-frame-pointer
COVERAGE_CXXFLAGS ?= -fprofile-instr-generate -fcoverage-mapping

ifeq ($(ARCH),x86_64)
RELEASE_CXXFLAGS += -march=x86-64-v2
endif

# -----------------------------------------------------------------------------
# Clang lifetime diagnostics
# -----------------------------------------------------------------------------

# Emit the warning flag only when the selected Clang understands it. This keeps
# the Makefile usable with older Clang releases while automatically enabling new
# lifetime-safety analysis as the compiler gains support.
define clang_warning_if_supported
$(strip $(shell printf 'int main(){}\n' | $(CXX) -x c++ -std=c++20 -fsyntax-only \
	-Werror=unknown-warning-option $(1) - >/dev/null 2>&1 && printf '%s' '$(1)'))
endef

LIFETIME_PERMISSIVE_FLAG := $(call clang_warning_if_supported,-Wlifetime-safety-permissive)
LIFETIME_STRICT_FLAG := $(call clang_warning_if_supported,-Wlifetime-safety-strict)
LIFETIME_VALIDATIONS_FLAG := $(call clang_warning_if_supported,-Wlifetime-safety-validations)
LIFETIME_SUGGESTIONS_FLAG := $(call clang_warning_if_supported,-Wlifetime-safety-suggestions)

# High-confidence diagnostics are part of every normal Debug compilation.
BASE_LIFETIME_CXXFLAGS := -Wdangling -Werror=dangling -Werror=return-stack-address
DEBUG_LIFETIME_CXXFLAGS := $(BASE_LIFETIME_CXXFLAGS) $(LIFETIME_PERMISSIVE_FLAG)

# The dedicated lifetime-check additionally enables diagnostics that may be
# noisier or intentionally advisory. -Wlifetime-safety-strict already includes
# invalidation analysis when supported by the compiler.
LIFETIME_CHECK_CXXFLAGS := \
	$(LIFETIME_STRICT_FLAG) \
	$(LIFETIME_VALIDATIONS_FLAG) \
	$(LIFETIME_SUGGESTIONS_FLAG)

# -----------------------------------------------------------------------------
# Optional features, packages and test selection
# -----------------------------------------------------------------------------

PROJECT_CPPFLAGS :=
PACKAGE_NAMES := krb5 krb5-gssapi zlib openssl

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

# -----------------------------------------------------------------------------
# Build flags
# -----------------------------------------------------------------------------

COMMON_CXXFLAGS := \
	-Wall -Wextra \
	-Wno-unused-command-line-argument \
	-Wno-unused-parameter \
	-Wno-unused-const-variable \
	-Wno-vla-extension \
	-Wno-deprecated-declarations \
	-std=c++20 \
	$(CXXFLAGS)

COMMON_LDFLAGS := -fuse-ld=lld $(LDFLAGS)
PACKAGE_CXXFLAGS := $(shell pkg-config --cflags $(PACKAGE_NAMES))
PACKAGE_LDLIBS := $(shell pkg-config --libs $(PACKAGE_NAMES))
LIB_CXXFLAGS := -fPIC $(PACKAGE_CXXFLAGS)
EXE_CXXFLAGS := -fPIE
LIB_LINK_FLAGS := -Wl,--no-undefined
GTEST_BUILD_CXXFLAGS := \
	-Wno-unused-command-line-argument \
	-fuse-ld=lld \
	-fPIC \
	$(CXXFLAGS) \
	$(GTEST_CXXFLAGS)

RELEASE_BUILD_CXXFLAGS := $(RELEASE_CXXFLAGS)
DEBUG_BUILD_CXXFLAGS := \
	$(DEBUG_CXXFLAGS) \
	$(DEBUG_LIFETIME_CXXFLAGS) \
	$(LIFETIME_EXTRA_CXXFLAGS) \
	$(DEBUG_COVERAGE_CXXFLAGS)

RELEASE_TEST_CXXFLAGS := $(RELEASE_BUILD_CXXFLAGS) $(TEST_CXXFLAGS)
DEBUG_TEST_CXXFLAGS := $(DEBUG_BUILD_CXXFLAGS) $(TEST_CXXFLAGS)

# -----------------------------------------------------------------------------
# Paths and generated files
# -----------------------------------------------------------------------------

GEN_DIR ?= gen
OUT_DIR ?= $(GEN_DIR)/$(ARCH)
GENERATED_INCLUDE_DIR ?= $(GEN_DIR)/include
GENERATED_EL1_INCLUDE_DIR := $(GENERATED_INCLUDE_DIR)/el1
LIFETIME_OUT_DIR ?= $(OUT_DIR)/lifetime
RELEASE_DIR := $(OUT_DIR)/release
DEBUG_DIR := $(OUT_DIR)/debug
GTEST_DIR := $(OUT_DIR)/gtest
COVERAGE_DIR := $(DEBUG_DIR)/coverage
COVERAGE_PROFILE_DIR := $(COVERAGE_DIR)/profiles
COVERAGE_HTML_DIR := $(COVERAGE_DIR)/html
COVERAGE_DATA := $(COVERAGE_DIR)/coverage.profdata
TEST_WORK_DIRS ?= $(OUT_DIR)/test.tmp $(OUT_DIR)/test1.tmp
TEST_OUT_DIR_DEFINE := '-DEL1_TEST_OUT_DIR=U"$(OUT_DIR)"'

LIB_DIR ?= /usr/lib
INCLUDE_DIR ?= /usr/include
PKG_CONFIG_DIR ?= $(LIB_DIR)/pkgconfig
PKG_CONFIG_LIB_DIR ?= $(LIB_DIR)
PKG_CONFIG_INCLUDE_DIR ?= $(INCLUDE_DIR)

ifneq ($(origin ABI_VERSION),undefined)
ifneq ($(strip $(ABI_VERSION)),$(PACKAGE_VERSION))
$(error ABI_VERSION='$(ABI_VERSION)' must equal PACKAGE_VERSION='$(PACKAGE_VERSION)')
endif
endif
override ABI_VERSION := $(PACKAGE_VERSION)
KEYID ?= BE5096C665CA4595AF11DAB010CD9FF74E4565ED

RPM_OUT_DIR := $(OUT_DIR)/rpm
ARCH_RPM_NAME := $(RPM_OUT_DIR)/el1.$(ARCH).rpm
DEVEL_RPM_NAME := $(RPM_OUT_DIR)/el1-devel.$(ARCH).rpm
SRC_RPM_NAME := $(RPM_OUT_DIR)/el1.src.rpm
DEVEL_SRC_RPM_NAME := $(RPM_OUT_DIR)/el1-devel.src.rpm
SPEC_NAME := el1.spec
DEVEL_SPEC_NAME := el1-devel.spec

LIB_BASENAME := libel1.so
LIB_SONAME := $(LIB_BASENAME).$(ABI_VERSION)
RELEASE_LIB_NAME := $(RELEASE_DIR)/$(LIB_SONAME)
DEBUG_LIB_NAME := $(DEBUG_DIR)/$(LIB_SONAME)
RELEASE_LIB_LINK_NAME := $(RELEASE_DIR)/$(LIB_BASENAME)
DEBUG_LIB_LINK_NAME := $(DEBUG_DIR)/$(LIB_BASENAME)
RELEASE_TEST_NAME := $(RELEASE_DIR)/gtest.exe
DEBUG_TEST_NAME := $(DEBUG_DIR)/gtest.exe
GTEST_LIB := $(GTEST_DIR)/lib/libgtest.a
GTEST_MAIN_LIB := $(GTEST_DIR)/lib/libgtest_main.a

LIB_SOURCES := $(wildcard src/el1/*.cpp)
LIB_HEADERS := $(wildcard src/el1/*.hpp)
GENERATED_LIB_HEADERS := $(patsubst src/el1/%.hpp,$(GENERATED_EL1_INCLUDE_DIR)/%.hpp,$(LIB_HEADERS))
SUPER_HEADER := $(GENERATED_EL1_INCLUDE_DIR)/el1.hpp
TEST_SOURCES := $(wildcard src/el1/test/*.cpp)

ifeq ($(WITH_BLUETOOTH),0)
LIB_SOURCES := $(filter-out src/el1/io_net_bluetooth.linux.cpp,$(LIB_SOURCES))
endif
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

# -----------------------------------------------------------------------------
# Test and coverage tools
# -----------------------------------------------------------------------------

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

# -----------------------------------------------------------------------------
# Primary targets
# -----------------------------------------------------------------------------

.PHONY: all headers release debug clean clean-all \
	compile-debug build-tests-release build-tests-debug \
	install install-runtime install-devel package rpm deploy \
	test test-release test-debug coverage-report \
	examples examples-test \
	check-valgrind check-coverage-tools lifetime-check entr

all: release debug

headers: $(GENERATED_LIB_HEADERS) $(SUPER_HEADER)
	@for header in "$(GENERATED_EL1_INCLUDE_DIR)"/*.hpp; do \
		[ -e "$$header" ] || continue; \
		[ "$$header" = "$(SUPER_HEADER)" ] && continue; \
		[ -f "src/el1/$${header##*/}" ] || rm -f -- "$$header"; \
	done

release: $(RELEASE_LIB_NAME) $(RELEASE_LIB_LINK_NAME) headers

debug: $(DEBUG_LIB_NAME) $(DEBUG_LIB_LINK_NAME) headers

# Compile every el1 Debug translation unit without linking googletest. This is
# used by static-analysis targets that only need compiler diagnostics.
compile-debug: $(DEBUG_LIB_OBJECTS) $(DEBUG_TEST_OBJECTS)

build-tests-release: $(RELEASE_TEST_NAME)

build-tests-debug: $(DEBUG_TEST_NAME)

examples: release
	$(MAKE) --no-print-directory -C examples \
		CXX="$(CXX)" ARCH="$(ARCH)" EL1_OUT_DIR="$(abspath $(OUT_DIR))" all

examples-test: release
	$(MAKE) --no-print-directory -C examples \
		CXX="$(CXX)" ARCH="$(ARCH)" EL1_OUT_DIR="$(abspath $(OUT_DIR))" smoke-test

# High-confidence lifetime diagnostics are already enabled by every Debug build.
# This target adds stricter/advisory analyses in an isolated output directory and
# verifies that deliberately-invalid lifetime examples are still rejected.
lifetime-check:
	$(MAKE) --no-print-directory \
		OUT_DIR="$(LIFETIME_OUT_DIR)" \
		WITH_POSTGRES=0 WITH_POSTGRES_TESTS=0 \
		WITH_VALGRIND=0 WITH_COVERAGE=0 \
		WITH_PROCESS_TESTS=0 WITH_NETWORK_TESTS=0 WITH_TIMING_TESTS=0 \
		LIFETIME_EXTRA_CXXFLAGS="$(LIFETIME_CHECK_CXXFLAGS)" \
		compile-debug
	@log="$$(mktemp)"; \
	if $(CXX) -std=c++20 -I src $(BASE_LIFETIME_CXXFLAGS) -fdiagnostics-show-option \
		-fsyntax-only support/lifetime-negative.cpp >"$$log" 2>&1; then \
		cat "$$log"; \
		rm -f "$$log"; \
		echo "lifetime-check: negative test unexpectedly compiled" >&2; \
		exit 1; \
	fi; \
	grep -Eq -- '-W(dangling[^]]*|return-stack-address|lifetime-safety[^]]*)' "$$log" || { \
		cat "$$log"; \
		rm -f "$$log"; \
		echo "lifetime-check: expected lifetime diagnostics missing" >&2; \
		exit 1; \
	}; \
	rm -f "$$log"
	@echo "Debug lifetime flags: $(strip $(DEBUG_LIFETIME_CXXFLAGS))"
	@echo "Additional lifetime-check flags: $(strip $(LIFETIME_CHECK_CXXFLAGS))"

clean:
	rm -rf -- "$(OUT_DIR)"

clean-all:
	rm -rf -- "$(GEN_DIR)" *.rpm vgcore.*

# -----------------------------------------------------------------------------
# Library and test build rules
# -----------------------------------------------------------------------------

$(GENERATED_EL1_INCLUDE_DIR)/%.hpp: src/el1/%.hpp
	@mkdir -p "$(@D)"
	@tmp="$$(mktemp "$@.tmp.XXXXXX")"; \
	trap 'rm -f -- "$$tmp"' EXIT HUP INT TERM; \
	install -m 644 "$<" "$$tmp"; \
	mv -f -- "$$tmp" "$@"

$(SUPER_HEADER): $(LIB_HEADERS)
	@mkdir -p "$(@D)"
	@tmp="$$(mktemp "$@.tmp.XXXXXX")"; \
	trap 'rm -f -- "$$tmp"' EXIT HUP INT TERM; \
	{ \
		echo "#pragma once"; \
		for header in $(LIB_HEADERS); do echo "#include \"$${header#src/el1/*}\""; done; \
	} > "$$tmp"; \
	mv -f -- "$$tmp" "$@"

$(RELEASE_DIR)/obj/%.o: src/el1/%.cpp Makefile
	@mkdir -p "$(@D)"
	$(CXX) $(CPPFLAGS) $(PROJECT_CPPFLAGS) $(COMMON_CXXFLAGS) $(RELEASE_BUILD_CXXFLAGS) $(LIB_CXXFLAGS) \
		-MMD -MP -c -o "$@" "$<"

$(DEBUG_DIR)/obj/%.o: src/el1/%.cpp Makefile
	@mkdir -p "$(@D)"
	$(CXX) $(CPPFLAGS) $(PROJECT_CPPFLAGS) $(COMMON_CXXFLAGS) $(DEBUG_BUILD_CXXFLAGS) $(LIB_CXXFLAGS) \
		-MMD -MP -c -o "$@" "$<"

$(RELEASE_LIB_NAME): $(RELEASE_LIB_OBJECTS) $(LIB_HEADERS)
	@mkdir -p "$(@D)"
	$(CXX) $(COMMON_LDFLAGS) $(RELEASE_CXXFLAGS) $(LIB_LINK_FLAGS) \
		-shared -Wl,-soname,$(LIB_SONAME) -o "$@" $(RELEASE_LIB_OBJECTS) $(PACKAGE_LDLIBS)

$(RELEASE_LIB_LINK_NAME): $(RELEASE_LIB_NAME)
	ln -sfn "$(notdir $(RELEASE_LIB_NAME))" "$@"

$(DEBUG_LIB_NAME): $(DEBUG_LIB_OBJECTS) $(LIB_HEADERS)
	@mkdir -p "$(@D)"
	$(CXX) $(COMMON_LDFLAGS) $(DEBUG_CXXFLAGS) $(DEBUG_COVERAGE_CXXFLAGS) $(LIB_LINK_FLAGS) \
		-shared -Wl,-soname,$(LIB_SONAME) -o "$@" $(DEBUG_LIB_OBJECTS) $(PACKAGE_LDLIBS)

$(DEBUG_LIB_LINK_NAME): $(DEBUG_LIB_NAME)
	ln -sfn "$(notdir $(DEBUG_LIB_NAME))" "$@"

$(RELEASE_DIR)/test/%.o: src/el1/test/%.cpp Makefile
	@mkdir -p "$(@D)"
	$(CXX) $(CPPFLAGS) $(PROJECT_CPPFLAGS) $(COMMON_CXXFLAGS) $(RELEASE_TEST_CXXFLAGS) \
		$(EXE_CXXFLAGS) $(PACKAGE_CXXFLAGS) "-DVERSION=\"$(VERSION)\"" $(TEST_OUT_DIR_DEFINE) \
		-I submodules/googletest/googletest/include -I src -MMD -MP -c -o "$@" "$<"

$(DEBUG_DIR)/test/%.o: src/el1/test/%.cpp Makefile
	@mkdir -p "$(@D)"
	$(CXX) $(CPPFLAGS) $(PROJECT_CPPFLAGS) $(COMMON_CXXFLAGS) $(DEBUG_TEST_CXXFLAGS) \
		$(EXE_CXXFLAGS) $(PACKAGE_CXXFLAGS) "-DVERSION=\"$(VERSION)\"" $(TEST_OUT_DIR_DEFINE) \
		-I submodules/googletest/googletest/include -I src -MMD -MP -c -o "$@" "$<"

$(GTEST_LIB): Makefile
	@mkdir -p "$(GTEST_DIR)"
	(P="$$PWD"; cd "$(GTEST_DIR)" && \
		cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="$(GTEST_BUILD_CXXFLAGS)" "$$P/submodules/googletest" && \
		$(MAKE))

$(GTEST_MAIN_LIB): $(GTEST_LIB)
	@test -f "$@"

$(RELEASE_TEST_NAME): $(RELEASE_TEST_OBJECTS) $(GTEST_LIB) $(GTEST_MAIN_LIB) $(RELEASE_LIB_LINK_NAME) $(SUPER_HEADER)
	@mkdir -p "$(@D)"
	$(CXX) $(COMMON_LDFLAGS) $(RELEASE_CXXFLAGS) $(EXE_CXXFLAGS) -o "$@" \
		$(RELEASE_TEST_OBJECTS) $(GTEST_LIB) $(GTEST_MAIN_LIB) \
		-L"$(RELEASE_DIR)" -lel1 $(PACKAGE_LDLIBS) $(EXEFLAGS)

$(DEBUG_TEST_NAME): $(DEBUG_TEST_OBJECTS) $(GTEST_LIB) $(GTEST_MAIN_LIB) $(DEBUG_LIB_LINK_NAME) $(SUPER_HEADER)
	@mkdir -p "$(@D)"
	$(CXX) $(COMMON_LDFLAGS) $(DEBUG_CXXFLAGS) $(DEBUG_COVERAGE_CXXFLAGS) $(EXE_CXXFLAGS) -o "$@" \
		$(DEBUG_LIB_OBJECTS) $(DEBUG_TEST_OBJECTS) $(GTEST_LIB) $(GTEST_MAIN_LIB) \
		$(PACKAGE_LDLIBS) $(EXEFLAGS)

# -----------------------------------------------------------------------------
# Tests and coverage
# -----------------------------------------------------------------------------

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

# Keep Release and Debug runs sequential: both use TEST_WORK_DIRS.
test:
	$(MAKE) --no-print-directory test-release
	$(MAKE) --no-print-directory coverage-report

test-release: check-valgrind $(RELEASE_TEST_NAME)
	./support/generate-testdata.sh "$(OUT_DIR)"
	rm -rf -- $(TEST_WORK_DIRS) && mkdir -p -- $(TEST_WORK_DIRS)
	LD_LIBRARY_PATH="$(RELEASE_DIR)" $(TEST_RUNNER) "$(RELEASE_TEST_NAME)" $(TEST_GTEST_FLAGS)

test-debug: check-valgrind $(DEBUG_TEST_NAME)
	./support/generate-testdata.sh "$(OUT_DIR)"
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

# -----------------------------------------------------------------------------
# Packaging and installation
# -----------------------------------------------------------------------------

RUNTIME_RPM_SOURCE_FILES := $(LIB_SOURCES) $(LIB_HEADERS) $(SPEC_NAME) Makefile LICENSE.txt
DEVEL_RPM_SOURCE_FILES := $(LIB_HEADERS) $(DEVEL_SPEC_NAME) Makefile LICENSE.txt

$(ARCH_RPM_NAME) $(SRC_RPM_NAME) &: $(RELEASE_LIB_NAME) $(RELEASE_LIB_LINK_NAME) $(RUNTIME_RPM_SOURCE_FILES)
	@mkdir -p "$(RPM_OUT_DIR)"
	ensure-git-clean.sh
	easy-rpm.sh --debug --prebuilt --name el1 --version "$(PACKAGE_VERSION)" --spec "$(SPEC_NAME)" --outdir "$(RPM_OUT_DIR)" --plain --arch "$(ARCH)" -- $(RUNTIME_RPM_SOURCE_FILES)

$(DEVEL_RPM_NAME) $(DEVEL_SRC_RPM_NAME) &: $(GENERATED_LIB_HEADERS) $(SUPER_HEADER) $(DEVEL_RPM_SOURCE_FILES)
	@mkdir -p "$(RPM_OUT_DIR)"
	ensure-git-clean.sh
	easy-rpm.sh --debug --prebuilt --name el1-devel --version "$(PACKAGE_VERSION)" --spec "$(DEVEL_SPEC_NAME)" --outdir "$(RPM_OUT_DIR)" --plain --arch "$(ARCH)" -- $(DEVEL_RPM_SOURCE_FILES)

install: install-runtime install-devel

install-runtime: $(RELEASE_LIB_NAME)
	mkdir -p "$(LIB_DIR)"
	install -m 755 "$(RELEASE_LIB_NAME)" "$(LIB_DIR)/$(LIB_SONAME)"

install-devel: headers
	rm -rf -- "$(INCLUDE_DIR)/el1"
	mkdir -p "$(LIB_DIR)" "$(INCLUDE_DIR)/el1" "$(PKG_CONFIG_DIR)"
	ln -sfn "$(LIB_SONAME)" "$(LIB_DIR)/$(LIB_BASENAME)"
	install -m 644 $(GENERATED_LIB_HEADERS) "$(SUPER_HEADER)" "$(INCLUDE_DIR)/el1/"
	{ \
		printf 'libdir=%s\n' '$(PKG_CONFIG_LIB_DIR)'; \
		printf 'includedir=%s\n\n' '$(PKG_CONFIG_INCLUDE_DIR)'; \
		printf 'Name: el1\n'; \
		printf 'Description: Essentials Library v1\n'; \
		printf 'Version: %s\n' '$(PACKAGE_VERSION)'; \
		printf 'Libs: -L$${libdir} -lel1\n'; \
		printf 'Cflags: -I$${includedir}\n'; \
	} > "$(PKG_CONFIG_DIR)/el1.pc"

package: rpm

rpm: $(ARCH_RPM_NAME) $(SRC_RPM_NAME) $(DEVEL_RPM_NAME) $(DEVEL_SRC_RPM_NAME)

deploy: $(ARCH_RPM_NAME) $(SRC_RPM_NAME) $(DEVEL_RPM_NAME) $(DEVEL_SRC_RPM_NAME)
	ensure-git-clean.sh
	deploy-rpm.sh --infile="$(SRC_RPM_NAME)" --outdir="$(RPMDIR)" --keyid="$(KEYID)"
	deploy-rpm.sh --infile="$(ARCH_RPM_NAME)" --outdir="$(RPMDIR)" --keyid="$(KEYID)"
	deploy-rpm.sh --infile="$(DEVEL_SRC_RPM_NAME)" --outdir="$(RPMDIR)" --keyid="$(KEYID)"
	deploy-rpm.sh --infile="$(DEVEL_RPM_NAME)" --outdir="$(RPMDIR)" --keyid="$(KEYID)"

# -----------------------------------------------------------------------------
# Developer convenience
# -----------------------------------------------------------------------------

entr: $(LIB_HEADERS) $(LIB_SOURCES) $(TEST_SOURCES) $(TEST_HEADERS)
	printf '%s\n' $^ | entr bash -c 'clear; reset; make test'

-include $(DEP_FILES)
