
#############################################################
# Required variables for each makefile
# Discard this section from all parent makefiles
# Expected variables (with automatic defaults):
#   CSRCS (all "C" files in the dir)
#   SUBDIRS (all subdirs with a Makefile)
#   GEN_LIBS - list of libs to be generated ()
#   GEN_IMAGES - list of images to be generated ()
#   COMPONENTS_xxx - a list of libs/objs in the form
#     subdir/lib to be extracted and rolled up into
#     a generated lib/image xxx.a ()
#
ifndef PDIR
UP_EXTRACT_DIR = ..
GEN_LIBS = librelib.a
COMPONENTS_librelib = \
	main/libmain.a \
	net80211/libnet80211.a
endif


#############################################################
# Configuration i.e. compile options etc.
# Target specific stuff (defines etc.) goes in here!
# Generally values applying to a tree are captured in the
#   makefile at its root level - these are then overridden
#   for a subtree within the makefile rooted therein
#
#DEFINES +=
#CCFLAGS +=
CCFLAGS += -std=gnu11
ifeq ($(LIBLWIP_VER), 2)
DEFINES += -DLIBLWIP_VER=2
DDEFINES += -DLIBLWIP_VER=2
LWIP2_ROOT := $(PWD)/../lwip2
export LWIP2_ROOT
INCLUDES += -I $${LWIP2_ROOT}/include -I $${LWIP2_ROOT}/src/include
else
DEFINES += -DLIBLWIP_VER=1
DDEFINES += -DLIBLWIP_VER=1
endif


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
INCLUDES += -I ./
PDIR := ../$(PDIR)
sinclude $(PDIR)Makefile

