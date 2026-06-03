################################################################################
#
# vyro-compositor — Vyro display server + window manager
#
################################################################################

VYRO_COMPOSITOR_VERSION = 0.1.0
VYRO_COMPOSITOR_SITE    = $(BR2_EXTERNAL_VYRO_PATH)/../compositor-drm
VYRO_COMPOSITOR_SITE_METHOD = local
VYRO_COMPOSITOR_LICENSE = MIT
VYRO_COMPOSITOR_DEPENDENCIES = libdrm libvyro-linux

define VYRO_COMPOSITOR_BUILD_CMDS
    $(TARGET_CC) $(TARGET_CFLAGS) -O2 -Wall \
        $(@D)/src/main.c \
        $(shell $(PKG_CONFIG_HOST_BINARY) --cflags libdrm) \
        $(shell $(PKG_CONFIG_HOST_BINARY) --libs libdrm) \
        -o $(@D)/vyro-compositor
endef

define VYRO_COMPOSITOR_INSTALL_TARGET_CMDS
    $(INSTALL) -D -m 0755 $(@D)/vyro-compositor \
        $(TARGET_DIR)/usr/bin/vyro-compositor
endef

$(eval $(generic-package))
