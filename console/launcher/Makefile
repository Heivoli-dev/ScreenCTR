# ScreenCTR Launcher — standard devkitPro 3DS application
ifeq ($(strip $(DEVKITARM)),)
$(error "DEVKITARM is not set")
endif
TOPDIR   ?= $(CURDIR)
include $(DEVKITARM)/3ds_rules

TARGET      := ScreenCTR-Launcher
BUILD       := build
SOURCES     := source
INCLUDES    :=
APP_TITLE   := ScreenCTR
APP_DESCRIPTION := Two screens. One connection.
APP_AUTHOR  := Heivoli Network
ICON        := ../../assets/icon48.png

ARCH     := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft
CFLAGS   := -g -Wall -O2 -mword-relocations -ffunction-sections $(ARCH) -D__3DS__ -I$(CTRULIB)/include
LDFLAGS  := -specs=3dsx.specs -g $(ARCH)
LIBS     := -lctru -lm
LIBDIRS  := $(CTRULIB)

# The rest is the stock devkitPro template logic (3dsx + smdh)
ifneq ($(BUILD),$(notdir $(CURDIR)))
export OUTPUT := $(CURDIR)/$(TARGET)
export TOPDIR := $(CURDIR)
export VPATH  := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir))
export DEPSDIR := $(CURDIR)/$(BUILD)
CFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
export OFILES := $(CFILES:.c=.o)
export LD := $(CC)
export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)
export _3DSXFLAGS += --smdh=$(CURDIR)/$(TARGET).smdh
export APP_ICON := $(TOPDIR)/$(ICON)
.PHONY: all clean
all: $(BUILD)
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile
$(BUILD):
	@mkdir -p $@
clean:
	rm -rf $(BUILD) $(TARGET).3dsx $(TARGET).smdh $(TARGET).elf
else
$(OUTPUT).3dsx: $(OUTPUT).elf $(OUTPUT).smdh
$(OUTPUT).elf: $(OFILES)
$(OUTPUT).smdh: $(APP_ICON)
	smdhtool --create "$(APP_TITLE)" "$(APP_DESCRIPTION)" "$(APP_AUTHOR)" $(APP_ICON) $@
endif
