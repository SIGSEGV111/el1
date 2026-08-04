EL1_ROOT := ..
OUT_DIR ?= $(EL1_ROOT)/gen/examples
CXX := $(shell command -v clang++ || command -v g++)

CPPFLAGS ?=
CXXFLAGS ?=
LDFLAGS ?=

COMMON_CPPFLAGS := -I$(EL1_ROOT)/src $(CPPFLAGS)
COMMON_CXXFLAGS := -std=c++20 -Wall -Wextra -Wno-unused-parameter -O2 -g $(CXXFLAGS)
COMMON_LDFLAGS := -L$(EL1_ROOT)/gen/release -Wl,-rpath,'$$ORIGIN/../release' $(LDFLAGS)
COMMON_LIBS := -lel1 $(shell pkg-config --libs krb5 zlib) -pthread

EXAMPLES := \
	ads111x \
	bin2cpp \
	dcf77-gpio \
	gpio-blink \
	gpio-trigger \
	hx711-test \
	json-tolerant-parser \
	neopixel-spi-driver \
	w1-test

SOURCES_ads111x := ads111x/ads111x.cpp
SOURCES_bin2cpp := bin2cpp/bin2cpp.cpp
SOURCES_dcf77-gpio := dcf77-gpio/dcf77-gpio.cpp
SOURCES_gpio-blink := gpio/blink.cpp
SOURCES_gpio-trigger := gpio/trigger.cpp
SOURCES_hx711-test := hx711/hx711-test.cpp
SOURCES_json-tolerant-parser := json-tolerant-parser/jtp.cpp
SOURCES_neopixel-spi-driver := neopixel-spi-driver/neopixel-spi-driver.cpp
SOURCES_w1-test := w1-test/w1-test.cpp

.SECONDEXPANSION:

.PHONY: all clean smoke-test $(EXAMPLES)

all: $(EXAMPLES)

$(EL1_ROOT)/gen/release/libel1.so:
	$(MAKE) -C $(EL1_ROOT) release

$(OUT_DIR):
	mkdir -p "$@"

$(EXAMPLES): %: $(OUT_DIR)/%

$(OUT_DIR)/%: $$(SOURCES_$$*) $(EL1_ROOT)/gen/release/libel1.so | $(OUT_DIR)
	$(CXX) $(COMMON_CPPFLAGS) $(COMMON_CXXFLAGS) "$<" -o "$@" $(COMMON_LDFLAGS) $(COMMON_LIBS)

smoke-test: all
	set -eu; \
	TMP_DIR="$$(mktemp -d)"; \
	trap 'rm -rf -- "$$TMP_DIR"' EXIT; \
	printf '\000\007\010\011\012\013\014\015\042\134\177\200' > "$$TMP_DIR/input.bin"; \
	"$(OUT_DIR)/bin2cpp" -i "$$TMP_DIR/input.bin" -o "$$TMP_DIR/output.hpp" -n TEST_DATA; \
	grep -q 'TEST_DATA' "$$TMP_DIR/output.hpp"; \
	printf '#include <el1/io_types.hpp>\n#include "output.hpp"\nint main() { return TEST_DATA_size == 12 && sizeof(TEST_DATA) == 13 ? 0 : 1; }\n' > "$$TMP_DIR/generated-test.cpp"; \
	$(CXX) $(COMMON_CPPFLAGS) -std=c++20 "$$TMP_DIR/generated-test.cpp" -I"$$TMP_DIR" -o "$$TMP_DIR/generated-test"; \
	"$$TMP_DIR/generated-test"; \
	printf '{"text":"line 1\nline 2"}' > "$$TMP_DIR/input.json"; \
	"$(OUT_DIR)/json-tolerant-parser" -i "$$TMP_DIR/input.json" -o "$$TMP_DIR/output.json"; \
	grep -q 'line 1' "$$TMP_DIR/output.json"; \
	for EXAMPLE in ads111x dcf77-gpio gpio-blink gpio-trigger hx711-test neopixel-spi-driver w1-test; do \
		"$(OUT_DIR)/$$EXAMPLE" --help >/dev/null 2>&1; \
	done

clean:
	rm -rf -- "$(OUT_DIR)"
