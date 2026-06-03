################################################################################
#
# vyro-init — Vyro PID 1
#
################################################################################

VYRO_INIT_VERSION = 0.1.0
VYRO_INIT_SITE    = $(BR2_EXTERNAL_VYRO_PATH)/../init
VYRO_INIT_SITE_METHOD = local
VYRO_INIT_LICENSE = MIT

define VYRO_INIT_BUILD_CMDS
    $(TARGET_CC) $(TARGET_CFLAGS) -O2 -static -Wall \
        $(@D)/init.c -o $(@D)/vyro-init
endef

define VYRO_INIT_INSTALL_TARGET_CMDS
    $(INSTALL) -D -m 0755 $(@D)/vyro-init $(TARGET_DIR)/sbin/vyro-init
    @# wire as PID 1
    $(LN) -sf /sbin/vyro-init $(TARGET_DIR)/sbin/init
endef

$(eval $(generic-package))
