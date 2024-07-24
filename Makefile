#
# Makefile for a Video Disk Recorder plugin
# 
# $Id: 2a41981a57e5e83036463c6a08c84b86ed9d2be3 $

# The official name of this plugin.
# This name will be used in the '-P...' option of VDR to load the plugin.
# By default the main source file also carries this name.


### Configuration (edit this for your needs)
#  comment out if not needed



CONFIG :=  -DDEBUG 		# remove # to enable debug output
#CONFIG +=  -DPERFTEST


# set to 1 if build in Kodi environment
KODIBUILD?=0


#--------------------- no more config needed past this point--------------------------------

# sanitize selections --------
ifneq "$(MAKECMDGOALS)" "clean"
ifneq "$(MAKECMDGOALS)" "indent"


endif # MAKECMDGOALS!=indent
endif # MAKECMDGOALS!=clean
#--------------------------

PLUGIN = softhdodroid

CONFIG += -DHAVE_GL			# needed for mpv libs
CONFIG += -DAV_INFO -DAV_INFO_TIME=3000	# info/debug a/v sync
CONFIG += -DUSE_MPEG_COMPLETE		# support only complete mpeg packets
CONFIG += -DUSE_VDR_SPU			# use VDR SPU decoder.
CONFIG += -DUSE_OPENGLOSD 

### The version number of this plugin (taken from the main source file):

VERSION = $(shell grep 'static const char \*const VERSION *=' softhdodroid.cpp | awk '{ print $$7 }' | sed -e 's/[";]//g')
GIT_REV = $(shell git describe --always 2>/dev/null)
### The name of the distribution archive:

### The directory environment:

# Use package data if installed...otherwise assume we're under the VDR source directory:
PKGCFG = $(if $(VDRDIR),$(shell pkg-config --variable=$(1) $(VDRDIR)/vdr.pc),$(shell PKG_CONFIG_PATH="$$PKG_CONFIG_PATH:../../.." pkg-config --variable=$(1) vdr))
LIBDIR = $(call PKGCFG,libdir)
LOCDIR = $(call PKGCFG,locdir)
PLGCFG = $(call PKGCFG,plgcfg)
#
TMPDIR ?= /tmp

### The compiler options:

export CFLAGS	= $(call PKGCFG,cflags) -I.
export CXXFLAGS = $(call PKGCFG,cxxflags)

ifeq ($(CFLAGS),)
$(warning CFLAGS not set)
endif
ifeq ($(CXXFLAGS),)
$(warning CXXFLAGS not set)
endif

### The version number of VDR's plugin API:

APIVERSION = $(call PKGCFG,apiversion)

### Allow user defined options to overwrite defaults:

-include $(PLGCFG)



### Parse softhddevice config

$(shell mkdir -p po)

_CFLAGS += $(shell pkg-config --cflags alsa)
LIBS += $(shell pkg-config --libs alsa)

_CFLAGS += $(shell pkg-config --cflags freetype2)
LIBS += $(shell pkg-config --libs freetype2)


#
# Test and set config for libcec 
#
ifeq (exists, $(shell pkg-config libcec && echo exists))
_CFLAGS += $(shell pkg-config --cflags libcec)
LIBS += $(shell pkg-config --libs libcec)
CONFIG += -DUSE_CEC 
OBJS = cec.o
else
OBJS = 
endif

_CFLAGS += $(shell pkg-config --cflags libdrm)
LIBS += $(shell pkg-config --libs libdrm)

ifeq ($(KODIBUILD),1)
_CFLAGS += $(shell pkg-config --cflags glesv2)
LIBS += $(shell pkg-config --libs glesv2)

_CFLAGS += $(shell pkg-config --cflags egl)
LIBS += $(shell pkg-config --libs egl)

_CFLAGS += $(shell pkg-config --cflags glm)
LIBS += $(shell pkg-config --libs glm)

_CFLAGS += $(shell pkg-config --cflags libdrm)
LIBS += $(shell pkg-config --libs libdrm)
LIBS +=  -ldl 
else
LIBS += -lMali -ldl -lGLU 
endif

ARCHIVE = $(PLUGIN)-$(VERSION)
PACKAGE = vdr-$(ARCHIVE)

### The name of the shared object file:

SOFILE = libvdr-$(PLUGIN).so


### Includes and Defines (add further entries here):

#INCLUDES += -I/usr/src/vdr/include

DEFINES += -DPLUGIN_NAME_I18N='"$(PLUGIN)"' -D_GNU_SOURCE $(CONFIG) \
	$(if $(GIT_REV), -DGIT_REV='"$(GIT_REV)"')

### Make it standard

override CXXFLAGS += $(_CFLAGS) $(DEFINES) $(INCLUDES) \
    -g  -Werror=overloaded-virtual
override CFLAGS	  += $(_CFLAGS) $(DEFINES) $(INCLUDES) \
    -g   -std=gnu99 

#
# Test and set config for libavutil 
#
ifneq (exists, $(shell pkg-config libavutil && echo exists))
  $(warning ******************************************************************)
  $(warning 'libavutil' not found!)
  $(error ******************************************************************)
endif
_CFLAGS += $(shell pkg-config --cflags libavutil)
LIBS += $(shell pkg-config --libs libavutil)

#
# Test and set config for libavcodec
#
ifneq (exists, $(shell pkg-config libavcodec && echo exists))
  $(warning ******************************************************************)
  $(warning 'libavcodec' not found!)
  $(error ******************************************************************)
endif
_CFLAGS += $(shell pkg-config --cflags libavcodec)
LIBS += $(shell pkg-config --libs libavcodec libavfilter)

CONFIG += -DUSE_ALSA
_CFLAGS += $(shell pkg-config --cflags alsa)
LIBS += $(shell pkg-config --libs alsa)

CONFIG += -DUSE_SWRESAMPLE
_CFLAGS += $(shell pkg-config --cflags libswresample)
LIBS += $(shell pkg-config --libs libswresample)


### The object files (add further files here):

OBJS += softhdodroid.o openglosd.o video.o softhddev.o audio.o ringbuffer.o codec.o 

SRCS = $(wildcard $(OBJS:.o=.c)) *.cpp

### The main target:

all: $(SOFILE) i18n

### Dependencies:

MAKEDEP = $(CXX) -MM -MG
DEPFILE = .dependencies
$(DEPFILE): Makefile
	@$(MAKEDEP) $(CXXFLAGS) $(SRCS) > $@

-include $(DEPFILE)

### Internationalization (I18N):

PODIR	  = po
I18Npo	  = $(wildcard $(PODIR)/*.po)
I18Nmo	  = $(addsuffix .mo, $(foreach file, $(I18Npo), $(basename $(file))))
I18Nmsgs  = $(addprefix $(DESTDIR)$(LOCDIR)/, $(addsuffix /LC_MESSAGES/vdr-$(PLUGIN).mo, $(notdir $(foreach file, $(I18Npo), $(basename $(file))))))
I18Npot	  = $(PODIR)/$(PLUGIN).pot

%.mo: %.po
	msgfmt -c -o $@ $<

$(I18Npot): $(SRCS)
	xgettext -C -cTRANSLATORS --no-wrap --no-location -k -ktr -ktrNOOP \
	-k_ -k_N --package-name=vdr-$(PLUGIN) --package-version=$(VERSION) \
	--msgid-bugs-address='<see README>' -o $@ `ls $^`

%.po: $(I18Npot)
	msgmerge -U --no-wrap --no-location --backup=none -q -N $@ $<
	@touch $@

$(I18Nmsgs): $(DESTDIR)$(LOCDIR)/%/LC_MESSAGES/vdr-$(PLUGIN).mo: $(PODIR)/%.mo
	install -D -m644 $< $@

.PHONY: i18n
i18n: $(I18Nmo) $(I18Npot)

install-i18n: $(I18Nmsgs)

### Targets:

$(OBJS): Makefile


$(SOFILE): $(OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -shared  $(OBJS) $(LIBS)  -o $@

install-lib: $(SOFILE)
	install -D $^ $(DESTDIR)$(LIBDIR)/$^.$(APIVERSION)

install: install-lib install-i18n

dist: $(I18Npo) clean
	@-rm -rf $(TMPDIR)/$(ARCHIVE)
	@mkdir $(TMPDIR)/$(ARCHIVE)
	@cp -a * $(TMPDIR)/$(ARCHIVE)
	@tar czf $(PACKAGE).tgz -C $(TMPDIR) $(ARCHIVE)
	@-rm -rf $(TMPDIR)/$(ARCHIVE)
	@echo Distribution package created as $(PACKAGE).tgz

clean:
	@-rm -f $(PODIR)/*.mo $(PODIR)/*.pot
	@-rm -f $(OBJS) $(DEPFILE) *.so *.tgz core* *~

## Private Targets:

HDRS=	$(wildcard *.h)

indent:
	for i in $(SRCS) $(HDRS); do \
		indent $$i; \
		unexpand -a $$i | sed -e s/constconst/const/ > $$i.up; \
		mv $$i.up $$i; \
	done

video_test: video.c Makefile
	$(CC) -DVIDEO_TEST -DVERSION='"$(VERSION)"' $(CFLAGS) $(LDFLAGS) $< \
	$(LIBS) -o $@
