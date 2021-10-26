PETITBOOT_M1_HOOK_SITE_METHOD = local
PETITBOOT_M1_HOOK_SITE = package/petitboot-m1-hook
PETITBOOT_M1_HOOK_DEPENDENCIES = dtc
PETITBOOT_M1_HOOK_LICENSE = GPLv2
PETITBOOT_M1_HOOK_DEPENDENCIES = dtc


define PETITBOOT_M1_HOOK_BUILD_CMDS
	$(TARGET_CC) $(TARGET_CFLAGS) $(TARGET_LDFLAGS) \
		 -o $(@D)/30-m1-hook $(@D)/transfer-live-props.c -lfdt
endef

define PETITBOOT_M1_HOOK_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/30-m1-hook $(TARGET_DIR)/etc/petitboot/boot.d/
endef

$(eval $(generic-package))
