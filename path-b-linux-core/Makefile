# Vyro OS — Path B — Linux Core build orchestrator
#
# Wraps Buildroot to produce a bootable disk image with the Linux kernel
# below and Vyro's compositor + window manager + apps above.
#
# Usage:
#   make              # full build
#   make qemu         # boot the image in qemu-system-x86_64
#   make clean        # remove build artifacts
#   make menuconfig   # buildroot config UI

VYRO_VERSION    ?= 7.0
ARCH            ?= x86_64
BUILDROOT_VER   ?= 2024.02

ROOT_DIR        := $(CURDIR)
BUILD_DIR       := $(ROOT_DIR)/build
BR_DIR          := $(BUILD_DIR)/buildroot-$(BUILDROOT_VER)
BR_TARBALL      := buildroot-$(BUILDROOT_VER).tar.gz
BR_URL          := https://buildroot.org/downloads/$(BR_TARBALL)
BR_EXTERNAL     := $(ROOT_DIR)/buildroot
DEFCONFIG       := vyro_$(ARCH)_defconfig
OUT_IMG         := $(BUILD_DIR)/vyro-core-$(VYRO_VERSION).img

.PHONY: all qemu clean distclean menuconfig info

all: $(OUT_IMG)

info:
	@echo "Vyro OS Core (Path B) — Linux + Vyro userland"
	@echo "  version:    $(VYRO_VERSION)"
	@echo "  arch:       $(ARCH)"
	@echo "  buildroot:  $(BUILDROOT_VER)"
	@echo "  output:     $(OUT_IMG)"

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/$(BR_TARBALL): | $(BUILD_DIR)
	cd $(BUILD_DIR) && wget -q $(BR_URL)

$(BR_DIR)/.extracted: $(BUILD_DIR)/$(BR_TARBALL)
	cd $(BUILD_DIR) && tar -xzf $(BR_TARBALL)
	touch $@

$(BR_DIR)/.configured: $(BR_DIR)/.extracted
	$(MAKE) -C $(BR_DIR) BR2_EXTERNAL=$(BR_EXTERNAL) $(DEFCONFIG)
	touch $@

$(OUT_IMG): $(BR_DIR)/.configured
	$(MAKE) -C $(BR_DIR) BR2_EXTERNAL=$(BR_EXTERNAL)
	cp $(BR_DIR)/output/images/disk.img $(OUT_IMG)
	@echo "==> OK: $(OUT_IMG)"
	@du -h $(OUT_IMG)

menuconfig: $(BR_DIR)/.configured
	$(MAKE) -C $(BR_DIR) BR2_EXTERNAL=$(BR_EXTERNAL) menuconfig

qemu: $(OUT_IMG)
	qemu-system-$(ARCH) \
	    -m 2G \
	    -smp 2 \
	    -enable-kvm \
	    -drive file=$(OUT_IMG),format=raw,if=virtio \
	    -vga virtio \
	    -display gtk,gl=on \
	    -device virtio-net-pci,netdev=net0 \
	    -netdev user,id=net0

clean:
	rm -rf $(BUILD_DIR)/buildroot-$(BUILDROOT_VER)/output

distclean:
	rm -rf $(BUILD_DIR)
