#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

PROJECT_NAME := home-intercom-esp8266-broker

EXTRA_CXXFLAGS := -std=c++17

include $(IDF_PATH)/make/project.mk
