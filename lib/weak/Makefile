WEAK_LIBS = \
	libairkiss.a \
	libat.a \
	libc.a \
	libcrypto.a \
	libdriver.a \
	libespnow.a \
	libgcc.a \
	libhal.a \
	libjson.a \
	liblwip.a \
	libmain.a \
	libmbedtls.a \
	libnet80211.a \
	libphy.a \
	libpp.a \
	libpwm.a \
	libsmartconfig.a \
	libssl.a \
	libupgrade.a \
	libwpa2.a \
	libwpa.a \
	libwps.a


# Default target for this subdir.
.PHONY: weaken
weaken: $(WEAK_LIBS)

.PHONY: preclean
preclean:
	rm -f *.a *.sym

clean: preclean

PDIR := ../../$(PDIR)
sinclude $(PDIR)Makefile


# Symbols with multiple definitions
KEEP_LOCAL = \
	mem_debug_file.* \
	ieee80211_hdrsize \
	ieee80211_is_11b_rate \
	wpa_derive_ptk \
	wpa_parse_generic

REDEFINE_ETS_SYMS = \
	bzero \
	memcmp \
	memcpy \
	memset \
	memmove \
	strcmp \
	strcpy \
	strlen \
	strncmp \
	strncpy \
	strstr \
	strchr \
	strrchr \

OBJ_REMOVE_libmain.a = mem_manager.o \
	eagle_lwip_if.o \
	ets_timer.o \
	eagle_lib.o \
	app_partition.o \
	spi_flash.o

OBJ_REMOVE_libpp.a = esf_buf.o

EMPTY :=
SPACE := $(EMPTY) $(EMPTY)
KEEP_LOCAL_RE = $(subst $(SPACE),|,$(KEEP_LOCAL))
REDEFINE_SYMS_F = $(shell for s in $(REDEFINE_ETS_SYMS); do echo "--redefine-sym ets_$$s=$$s"; done) \
	--redefine-sym=hostap_deliver_data=ets_hostap_deliver_data \
	--redefine-sym=ieee80211_deliver_data=ets_ieee80211_deliver_data \
	--redefine-sym=pbuf_alloc=lwip1_pbuf_alloc \
	--redefine-sym=pbuf_free=lwip1_pbuf_free

RENAME_SECTIONS = \
	--rename-section=.text.hostap_deliver_data=.stub \
	--rename-section=.text.ieee80211_deliver_data=.stub


# Globalize and weaken the symbols
%.a: ../%.a Makefile
	$(NM) -P --defined-only $< | \
		grep -E -v "(:|[$$])" | \
		cut -d" " -f1 | \
		grep -E -v "^($(KEEP_LOCAL_RE))$$" | \
		sort -u > $(@:.a=.sym)
	$(OBJCOPY) --globalize-symbols $(@:.a=.sym) $< tmp1.a
	$(OBJCOPY) --weaken tmp1.a tmp2.a
	$(OBJCOPY) $(REDEFINE_SYMS_F) $(RENAME_SECTIONS) tmp2.a $@
	$(AR) dv $@ $(OBJ_REMOVE_$(@))
