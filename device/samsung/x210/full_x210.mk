PRODUCT_PACKAGES := \
    Camera \
    VoiceDialer \
    GestureBuilder \
    librs_jni \
    libRS \
    LiveWallpapersPicker \
    LiveWallpapers \
    MagicSmokeWallpapers \
    VisualizationWallpapers \
    hwcomposer.default

# Inherit from those products. Most specific first.
$(call inherit-product, $(SRC_TARGET_DIR)/product/full_base.mk)
$(call inherit-product, device/samsung/x210/device.mk)

# Overrides
PRODUCT_NAME := full_x210
PRODUCT_DEVICE := x210
PRODUCT_BRAND := Android
PRODUCT_MODEL := Android On X210
