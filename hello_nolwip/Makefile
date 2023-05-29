#############################################################
# Required variables for each makefile
# Discard this section from all parent makefiles
# Expected variables (with automatic defaults):
#   CSRCS (all "C" files in the dir)
#   SUBDIRS (all subdirs with a Makefile)
#   GEN_LIBS - list of libs to be generated ()
#   GEN_IMAGES - list of object file images to be generated ()
#   GEN_BINS - list of binaries to be generated ()
#   COMPONENTS_xxx - a list of libs/objs in the form
#     subdir/lib to be extracted and rolled up into
#     a generated lib/image xxx.a ()
#
TARGET = eagle
#FLAVOR = release
FLAVOR = debug

COMPILE      ?= gcc
SPI_SIZE_MAP ?= 4  # 4MiB / 16Mbit
export COMPILE SPI_SIZE_MAP

#EXTRA_CCFLAGS += -u

ifndef PDIR # {
GEN_IMAGES= eagle.app.v6.out
GEN_BINS= eagle.app.v6.bin
SPECIAL_MKTARGETS=$(APP_MKTARGETS)
SUBDIRS= \
    ../lib/weak \
    ../third_party/relib \
    user \
    lwipstubs \

endif # } PDIR

APPDIR = .
LDDIR = ../ld

CCFLAGS += -Os -Werror

TARGET_LDFLAGS = \
    -nostdlib \
    -Wl,-EL \
    --longcalls \
    --text-section-literals

ifeq ($(FLAVOR),debug)
    TARGET_LDFLAGS += -g -O2
endif

ifeq ($(FLAVOR),release)
    TARGET_LDFLAGS += -g -O0
endif

COMPONENTS_eagle.app.v6 = \
    user/libuser.a \
    lwipstubs/liblwipstubs.a \
    ../third_party/relib/librelib.a \

LINKFLAGS_eagle.app.v6 = \
    -L../lib/weak \
    -nostdlib \
    -T$(LD_FILE) \
    -Wl,-Map=eagle.app.map \
    -Wl,--verbose \
    -Wl,--no-check-sections \
    -Wl,--gc-sections \
    -u call_user_start \
    -Wl,-static \
    -Wl,--start-group \
    $(DEP_LIBS_eagle.app.v6) \
    -lc \
    -lgcc \
    -lhal \
    -lphy \
    -lpp \
    -lnet80211 \
    -lwpa \
    -lcrypto \
    -lmain \
    -ldriver \
    -Wl,--end-group

DEPENDS_eagle.app.v6 = \
                $(LD_FILE) \
                $(LDDIR)/eagle.rom.addr.v6.ld

#############################################################
# Configuration i.e. compile options etc.
# Target specific stuff (defines etc.) goes in here!
# Generally values applying to a tree are captured in the
#   makefile at its root level - these are then overridden
#   for a subtree within the makefile rooted therein
#

#UNIVERSAL_TARGET_DEFINES =     \

# Other potential configuration flags include:
#   -DTXRX_TXBUF_DEBUG
#   -DTXRX_RXBUF_DEBUG
#   -DWLAN_CONFIG_CCX
CONFIGURATION_DEFINES = -DICACHE_FLASH -DMEMLEAK_DEBUG=1

DEFINES += \
    $(UNIVERSAL_TARGET_DEFINES) \
    $(CONFIGURATION_DEFINES)

DDEFINES += \
    $(UNIVERSAL_TARGET_DEFINES) \
    $(CONFIGURATION_DEFINES)


#############################################################
# Recursion Magic - Don't touch this!!
#
# Each subtree potentially has an include directory
#   corresponding to the common APIs applicable to modules
#   rooted at that subtree. Accordingly, the INCLUDE PATH
#   of a module can only contain the include directories up
#   its parent path, and not its siblings
#
# Required for each makefile to inherit from the parent
#

INCLUDES := $(INCLUDES) -I $(PDIR)include
PDIR := ../$(PDIR)
sinclude $(PDIR)Makefile

.PHONY: FORCE
FORCE:

.PHONY: flash
flash:
	esptool -p /dev/ttyUSB0 write_flash 0x0 ../bin/eagle.flash.bin 0x10000 ../bin/eagle.irom0text.bin

$(IMAGEODIR)/iram.out: $(OIMAGES)
	$(OBJCOPY) --only-section=.text $< $@

iramsize.txt: $(IMAGEODIR)/iram.out
	$(NM) --size-sort $< > $@
