.PHONY: all clean install package rpm test

ARCH ?= $(shell rpm --eval '%{_target_cpu}')
CXXFLAGS ?=

ifeq ($(ARCH),x86_64)
	CXXFLAGS += -march=x86-64-v2
endif

VERSION ?= *DEVELOPMENT SNAPSHOT*
EL1_CXX ?= $(shell which clang++)
EL1_CXXFLAGS := "-DVERSION=\"$(VERSION)\"" -Wall -Wextra -Wno-unused-parameter -Wno-unused-const-variable -Wno-vla-extension -DEL1_WITH_POSTGRES -std=c++20 -O3 -g -flto -fPIC $(shell pkg-config --cflags libpq) $(shell pkg-config --cflags krb5) $(shell pkg-config --cflags zlib) $(CXXFLAGS)
LIB_LDFLAGS := $(shell pkg-config --libs libpq) $(shell pkg-config --libs krb5) $(shell pkg-config --libs zlib)

OUT_DIR ?= gen
LIB_DIR ?= /usr/lib
INCLUDE_DIR ?= /usr/include
KEYID ?= BE5096C665CA4595AF11DAB010CD9FF74E4565ED
ARCH_RPM_NAME := el1.$(ARCH).rpm
SRC_RPM_NAME := el1.src.rpm
SPEC_NAME := el1.spec

LIB_NAME := $(OUT_DIR)/libel1.so
TEST_NAME := $(OUT_DIR)/gtest.exe
SUPER_HEADER := $(OUT_DIR)/el1.hpp

LIB_SOURCES := $(wildcard src/el1/*.cpp)
LIB_HEADERS := $(wildcard src/el1/*.hpp)
LIB_OBJECTS := $(patsubst src/el1/%.cpp,$(OUT_DIR)/%.o,$(LIB_SOURCES))
DEP_FILES   := $(patsubst src/el1/%.cpp,$(OUT_DIR)/%.d,$(LIB_SOURCES))

TEST_SOURCES := $(wildcard src/el1/test/*.cpp)
TEST_HEADERS := $(wildcard src/el1/test/*.hpp)
TEST_OBJECTS := $(patsubst src/el1/test/%.cpp,$(OUT_DIR)/test/%.o,$(TEST_SOURCES))

export EL1_CXX
export EL1_CXXFLAGS

all: $(LIB_NAME) $(SUPER_HEADER)

clean:
	rm -rf -- $(OUT_DIR)

$(SUPER_HEADER): $(LIB_HEADERS)
	@mkdir -p $(@D)
	echo "#pragma once" > $(SUPER_HEADER).tmp
	for header in $(LIB_HEADERS); do echo "#include \"$${header#src/el1/*}\""; done >> $(SUPER_HEADER).tmp
	mv -f -- $(SUPER_HEADER).tmp $(SUPER_HEADER)

$(OUT_DIR)/%.o: src/el1/%.cpp
	@mkdir -p $(@D)
	$(EL1_CXX) $(EL1_CXXFLAGS) -MMD -MP -c -o $@ $<

$(OUT_DIR)/test/%.o: src/el1/test/%.cpp
	@mkdir -p $(@D)
	$(EL1_CXX) $(EL1_CXXFLAGS) -I submodules/googletest/googletest/include -I src -MMD -MP -c -o $@ $<

$(OUT_DIR)/gtest/lib/libgtest.a:
	@mkdir -p "$(OUT_DIR)/gtest"
	(P="$$PWD"; cd "$(OUT_DIR)/gtest" && cmake "$$P/submodules/googletest" && make)

$(LIB_NAME): $(LIB_OBJECTS)
	@mkdir -p $(@D)
	$(EL1_CXX) -fuse-ld=lld $(EL1_CXXFLAGS) -o $@ $(LIB_OBJECTS) $(LIB_LDFLAGS) -shared

$(TEST_NAME): $(TEST_OBJECTS) $(OUT_DIR)/gtest/lib/libgtest.a $(LIB_NAME)
	@mkdir -p $(@D)
	$(EL1_CXX) $(EL1_CXXFLAGS) -o $@ $(TEST_OBJECTS) $(OUT_DIR)/gtest/lib/libgtest.a $(OUT_DIR)/gtest/lib/libgtest_main.a -L$(OUT_DIR) -lel1 $(EXEFLAGS)

$(ARCH_RPM_NAME) $(SRC_RPM_NAME): $(LIB_SOURCES) $(LIB_HEADERS) $(SERVICE_NAME) $(SPEC_NAME) $(CONF_NAME) Makefile
	easy-rpm.sh --debug --name el1 --spec $(SPEC_NAME) --outdir . --plain --arch "$(ARCH)" -- $^

install: $(LIB_NAME) $(LIB_HEADERS) $(SUPER_HEADER)
	rm -rf -- "$(INCLUDE_DIR)/el1"
	mkdir -p "$(LIB_DIR)" "$(INCLUDE_DIR)/el1"
	install -m 755 $(LIB_NAME) "$(LIB_DIR)/"
	install -m 644 $(LIB_HEADERS) $(SUPER_HEADER) "$(INCLUDE_DIR)/el1/"

deploy: $(ARCH_RPM_NAME)
	ensure-git-clean.sh
	deploy-rpm.sh --infile=$(SRC_RPM_NAME) --outdir="$(RPMDIR)" --keyid="$(KEYID)" --srpm
	deploy-rpm.sh --infile="$(ARCH_RPM_NAME)" --outdir="$(RPMDIR)" --keyid="$(KEYID)"

rpm: $(ARCH_RPM_NAME)

test: $(TEST_NAME)
	LD_LIBRARY_PATH=$(OUT_DIR) valgrind \
		--quiet \
		--leak-check=full \
		--show-reachable=no \
		--track-origins=yes \
		--num-callers=30 \
		--trace-children=no \
		--track-fds=yes \
		--error-exitcode=1 \
		--suppressions=support/valgrind.sup \
		--gen-suppressions=all \
		$(TEST_NAME)

-include $(DEP_FILES)
