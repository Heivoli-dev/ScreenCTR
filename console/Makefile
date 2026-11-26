# ScreenCTR console sysmodule — devkitARM + libctru + makerom
# Usage: make            -> build/ScreenCTR.cxi, ScreenCTR.cia
#        make launcher   -> launcher/ScreenCTR-Launcher.3dsx / .cia
.SUFFIXES:
ifeq ($(strip $(DEVKITARM)),)
$(error "DEVKITARM is not set: install devkitPro (3ds-dev) first, see docs/BUILD.md")
endif
include $(DEVKITARM)/3ds_rules

TARGET   := ScreenCTR
BUILD    := build
ROOT     := $(CURDIR)/..
SOURCES  := source $(ROOT)/protocol/src $(ROOT)/common/jpeg $(ROOT)/common/pixfmt $(ROOT)/common/config
INCLUDES := source $(ROOT)/protocol/include $(ROOT)/common/jpeg $(ROOT)/common/pixfmt $(ROOT)/common/config

ARCH     := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft
CFLAGS   := -g -std=gnu11 -Wall -Wextra -O2 -mword-relocations -ffunction-sections -fdata-sections \
            $(ARCH) -D__3DS__ $(foreach d,$(INCLUDES),-I$(d)) -I$(CTRULIB)/include
LDFLAGS  := -specs=3dsx.specs -g $(ARCH) -Wl,--gc-sections -Wl,-Map,$(BUILD)/$(TARGET).map
LIBS     := -L$(CTRULIB)/lib -lctru -lm

CFILES   := $(foreach d,$(SOURCES),$(wildcard $(d)/*.c))
OFILES   := $(patsubst %.c,$(BUILD)/%.o,$(notdir $(CFILES)))
VPATH    := $(SOURCES)

.PHONY: all clean launcher
all: $(TARGET).cia

$(BUILD):
	@mkdir -p $@

$(BUILD)/%.o: %.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/$(TARGET).elf: $(OFILES)
	$(CC) $(LDFLAGS) $^ $(LIBS) -o $@

$(BUILD)/$(TARGET).cxi: $(BUILD)/$(TARGET).elf screenctr.rsf
	makerom -f ncch -rsf screenctr.rsf -nocodepadding -o $@ -elf $<

$(TARGET).cia: $(BUILD)/$(TARGET).cxi
	makerom -f cia -o $@ -i $<:0:0 -ignoresign

launcher:
	$(MAKE) -C launcher

clean:
	rm -rf $(BUILD) $(TARGET).cia
	$(MAKE) -C launcher clean
