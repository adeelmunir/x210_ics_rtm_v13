# These is the hardware-specific overlay, which points to the location
# of hardware-specific resource overrides, typically the frameworks and
# application settings that are stored in resourced.
DEVICE_PACKAGE_OVERLAYS := device/samsung/x210/overlay

PRODUCT_COMMON_DIR := device/samsung/common/s5p

PRODUCT_COPY_FILES := \
	device/samsung/x210/init.x210.rc:root/init.x210.rc \
	device/samsung/x210/init.x210.usb.rc:root/init.x210.usb.rc \
	device/samsung/x210/ueventd.x210.rc:root/ueventd.x210.rc \
	device/samsung/x210/s3c-button.kl:system/usr/keylayout/s3c-button.kl \
	device/samsung/x210/s3c-button.kcm:system/usr/keychars/s3c-button.kcm \
	device/samsung/x210/s3c-button.idc:system/usr/idc/s3c-button.idc \
	device/samsung/x210/ft5x06-ts.kl:system/usr/keylayout/ft5x06-ts.kl \
	device/samsung/x210/ft5x06-ts.kcm:system/usr/keychars/ft5x06-ts.kcm \
	device/samsung/x210/ft5x06-ts.idc:system/usr/idc/ft5x06-ts.idc \
	device/samsung/x210/s3c_ts.idc:system/usr/idc/s3c_ts.idc \
	device/samsung/x210/vold.fstab:system/etc/vold.fstab

# These are the hardware-specific features
PRODUCT_COPY_FILES += \
	frameworks/base/data/etc/handheld_core_hardware.xml:system/etc/permissions/handheld_core_hardware.xml \
	frameworks/base/data/etc/android.hardware.camera.flash-autofocus.xml:system/etc/permissions/android.hardware.camera.flash-autofocus.xml \
	frameworks/base/data/etc/android.hardware.camera.front.xml:system/etc/permissions/android.hardware.camera.front.xml \
	frameworks/base/data/etc/android.hardware.location.gps.xml:system/etc/permissions/android.hardware.location.gps.xml \
	frameworks/base/data/etc/android.hardware.bluetooth.xml:system/etc/permissions/android.hardware.bluetooth.xml \
	frameworks/base/data/etc/android.hardware.sensor.proximity.xml:system/etc/permissions/android.hardware.sensor.proximity.xml \
	frameworks/base/data/etc/android.hardware.sensor.light.xml:system/etc/permissions/android.hardware.sensor.light.xml \
	frameworks/base/data/etc/android.hardware.sensor.gyroscope.xml:system/etc/permissions/android.hardware.sensor.gyroscope.xml \
	frameworks/base/data/etc/android.hardware.nfc.xml:system/etc/permissions/android.hardware.nfc.xml \
	frameworks/base/data/etc/android.software.sip.voip.xml:system/etc/permissions/android.software.sip.voip.xml \
	frameworks/base/data/etc/android.hardware.usb.accessory.xml:system/etc/permissions/android.hardware.usb.accessory.xml \
	frameworks/base/data/etc/android.hardware.usb.host.xml:system/etc/permissions/android.hardware.usb.host.xml \
	frameworks/base/data/etc/android.hardware.wifi.xml:system/etc/permissions/android.hardware.wifi.xml \
	frameworks/base/data/etc/android.hardware.wifi.direct.xml:system/etc/permissions/android.hardware.wifi.direct.xml \
	packages/wallpapers/LivePicker/android.software.live_wallpaper.xml:system/etc/permissions/android.software.live_wallpaper.xml

PRODUCT_PROPERTY_OVERRIDES += \
	ro.sf.lcd_density=200 \
	ro.opengles.version=131072

PRODUCT_CHARACTERISTICS := nosdcard

# Set default USB interface
PRODUCT_DEFAULT_PROPERTY_OVERRIDES += \
        persist.sys.usb.config=mtp

PRODUCT_PROPERTY_OVERRIDES += \
	wifi.interface=wlan0 \
	hwui.render_dirty_regions=false

PRODUCT_TAGS += dalvik.gc.type-precise

PRODUCT_PACKAGES += \
	gralloc.x210 

#audio
PRODUCT_PACKAGES += \
        audio_policy.x210 \
        audio.primary.x210 \
        audio.a2dp.default \
        lights.x210 \
        hwcomposer.x210 \
        libaudioutils \
        sensors.x210

# These is the OpenMAX IL configuration files
PRODUCT_COPY_FILES += \
	$(PRODUCT_COMMON_DIR)/sec_mm/sec_omx/sec_omx_core/secomxregistry:system/etc/secomxregistry \
	$(PRODUCT_COMMON_DIR)/media_profiles.xml:system/etc/media_profiles.xml

#MFC Firmware
PRODUCT_COPY_FILES += \
        $(PRODUCT_COMMON_DIR)/samsung_mfc_fw.bin:system/vendor/firmware/samsung_mfc_fw.bin

# Common Binary
PRODUCT_COPY_FILES +=   \
        device/samsung/x210/bin/busybox:system/bin/busybox \
        device/samsung/x210/bin/preinstall.sh:/system/bin/preinstall.sh

# Pre-installed apks
PRODUCT_COPY_FILES += \
        $(call find-copy-subdir-files,*.apk,$(LOCAL_PATH)/apk,system/preinstall)

# 3G dongle conf
PRODUCT_COPY_FILES += \
	device/samsung/x210/3g/usb_modeswitch.sh:system/etc/usb_modeswitch.sh \
	device/samsung/x210/3g/usb_modeswitch:system/bin/usb_modeswitch

PRODUCT_COPY_FILES += \
	$(call find-copy-subdir-files,*,device/samsung/x210/3g/usb_modeswitch.d,system/etc/usb_modeswitch.d)

# These are the OpenMAX IL modules
PRODUCT_PACKAGES += \
        libSEC_OMX_Core \
        libOMX.SEC.AVC.Decoder \
        libOMX.SEC.M4V.Decoder \
        libOMX.SEC.M4V.Encoder \
        libOMX.SEC.AVC.Encoder

# Include libstagefright module
PRODUCT_PACKAGES += \
	libstagefrighthw
# Camera
PRODUCT_PACKAGES += \
	camera.x210

# Filesystem management tools
PRODUCT_PACKAGES += \
	make_ext4fs \
	setup_fs

$(call inherit-product, frameworks/base/build/phone-xhdpi-1024-dalvik-heap.mk)
$(call inherit-product-if-exists, vendor/samsung/x210/device-vendor.mk)
