ifeq ($(TARGET),UNIX)
ENABLE_ALSA = y
else
# only unix comes with alsa
ENABLE_ALSA = n
endif

ENABLE_SDL = n

# $(eval $(call pkg-config-library,ALSA,alsa))

ALSA_CPPFLAGS += -DENABLE_ALSA
ALSA_LDLIBS += -lasound
