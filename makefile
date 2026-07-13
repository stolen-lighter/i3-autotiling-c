CC := zig cc
OUT_DIR := bin

ZIG_CHECK := $(shell command -v zig 2> /dev/null)
ifeq ($(ZIG_CHECK),)
    $(error "Error: 'zig' command not found. Please install the Zig toolchain to compile this project with Musl support.")
endif

UNAME_M := $(shell uname -m)
ifeq ($(UNAME_M),x86_64)
    TARGET_ARCH := -target x86_64-linux-musl
else ifeq ($(UNAME_M),aarch64)
    TARGET_ARCH := -target aarch64-linux-musl
else ifeq ($(UNAME_M),arm64)
    TARGET_ARCH := -target aarch64-linux-musl
else
    TARGET_ARCH :=
endif

CFLAGS := $(TARGET_ARCH) -Os -s -flto -ffunction-sections -fdata-sections -fno-ident -fno-asynchronous-unwind-tables -fomit-frame-pointer
LDFLAGS := -static -Wl,--gc-sections -Wl,--build-id=none -flto
SRC := autotiling.c
TARGET := $(OUT_DIR)/i3-autotiling-c
.DEFAULT_GOAL := all
all: $(TARGET)

$(TARGET): $(SRC) | $(OUT_DIR)
	$(CC) -idirafter /usr/include $(CFLAGS) $(LDFLAGS) $< -o $@

$(OUT_DIR):
	mkdir -p $(OUT_DIR)

.PHONY: clean install uninstall

install: $(TARGET)
	mkdir -p $(HOME)/.local/bin
	cp $(TARGET) $(HOME)/.local/bin/

clean:
	rm -rf $(OUT_DIR)

uninstall:
	rm -f $(HOME)/.local/bin/i3-autotiling-c
