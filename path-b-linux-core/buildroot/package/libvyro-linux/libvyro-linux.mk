################################################################################
#
# libvyro-linux — Vyro userland library, Linux port
#
################################################################################

LIBVYRO_LINUX_VERSION = 0.1.0
LIBVYRO_LINUX_SITE    = $(BR2_EXTERNAL_VYRO_PATH)/../libvyro-linux
LIBVYRO_LINUX_SITE_METHOD = local
LIBVYRO_LINUX_LICENSE = MIT
LIBVYRO_LINUX_LICENSE_FILES = LICENSE
LIBVYRO_LINUX_INSTALL_STAGING = YES
LIBVYRO_LINUX_DEPENDENCIES = libdrm libinput

define LIBVYRO_LINUX_BUILD_CMDS
    $(MAKE) CC="$(TARGET_CC)" AR="$(TARGET_AR)" -C $(@D)
endef

define LIBVYRO_LINUX_INSTALL_STAGING_CMDS
    $(INSTALL) -D -m 0644 $(@D)/libvyro.a    $(STAGING_DIR)/usr/lib/libvyro.a
    $(INSTALL) -D -m 0644 $(@D)/include/vyro.h $(STAGING_DIR)/usr/include/vyro.h
endef

define LIBVYRO_LINUX_INSTALL_TARGET_CMDS
    @# header-only at runtime; static lib not installed to target
    @true
endef

$(eval $(generic-package))
