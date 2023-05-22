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

EMPTY :=
SPACE := $(EMPTY) $(EMPTY)
KEEP_LOCAL_RE = $(subst $(SPACE),|,$(KEEP_LOCAL))
REDEFINE_SYMS_F = $(shell for s in $(REDEFINE_ETS_SYMS); do echo "--redefine-sym ets_$$s=$$s"; done)


# Globalize and weaken the symbols
%.a: ../%.a Makefile
	$(NM) -P --defined-only $< | \
		grep -E -v "(:|[$$])" | \
		cut -d" " -f1 | \
		grep -E -v "^($(KEEP_LOCAL_RE))$$" | \
		sort -u > $(@:.a=.sym)
	$(OBJCOPY) \
		$(REDEFINE_SYMS_F) \
		--globalize-symbols $(@:.a=.sym) \
		--weaken $< $@
