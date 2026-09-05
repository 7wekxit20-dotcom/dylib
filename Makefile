ARCHS = arm64
TARGET := iphone:clang:latest:14.0
INSTALL_TARGET_PROCESSES = FreeFire

include $(THEOS)/makefiles/common.mk

TWEAK_NAME = banBypass

banBypass_FILES = ban.cpp
banBypass_CFLAGS = -fobjc-arc
banBypass_CCFLAGS = -std=c++17
banBypass_LDFLAGS = -Wl,-segalign,4000

include $(THEOS_MAKE_PATH)/tweak.mk
