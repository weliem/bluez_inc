################################################################################
#
# bluez_inc
#
################################################################################

BLUEZ_INC_VERSION = main
BLUEZ_INC_SITE = https://github.com/weliem/bluez_inc.git
BLUEZ_INC_SITE_METHOD = git
BLUEZ_INC_INSTALL_STAGING = YES
BLUEZ_INC_LICENSE = MIT
BLUEZ_INC_LICENSE_FILES = LICENSE
BLUEZ_INC_DEPENDENCIES = host-pkgconf libglib2

BLUEZ_INC_CONF_OPTS += \
    -DPKG_CONFIG_EXECUTABLE="$(PKG_CONFIG_HOST_BINARY)" \
    -DPKG_CONFIG_PATH="$(STAGING_DIR)/usr/lib/pkgconfig:$(STAGING_DIR)/usr/share/pkgconfig"

$(eval $(cmake-package))