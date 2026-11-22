TOOLCHAIN := $(OO_PS4_TOOLCHAIN)
GOLDHEN_SDK := $(GOLDHEN_PLUGINS_SDK)

PROJDIR   := $(shell pwd)
SOURCEDIR := $(PROJDIR)/source
BUILDDIR  := $(PROJDIR)/build
TARGET    := ps4-timeSync.prx

CC           := clang
LD           := ld.lld
CREATE_FSELF := $(TOOLCHAIN)/bin/linux/create-fself

CFILES := $(wildcard $(SOURCEDIR)/*.c) $(wildcard $(PROJDIR)/*.c)
OBJS   := $(patsubst %.c, $(BUILDDIR)/%.o, $(notdir $(CFILES)))
VPATH  := $(SOURCEDIR):$(PROJDIR)

CFLAGS  := --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -I$(TOOLCHAIN)/include -I$(TOOLCHAIN)/include/c++/v1 -I$(GOLDHEN_SDK)/include -O2 -Wall
LDFLAGS := -m elf_x86_64 -shared --script $(TOOLCHAIN)/link.x --eh-frame-hdr -L$(TOOLCHAIN)/lib -L$(GOLDHEN_SDK) -lGoldHEN_Hook -lc -lkernel -lSceNet -lSceSysmodule

all: $(TARGET)

$(TARGET): $(BUILDDIR) $(OBJS)
	@if [ -z "$(OBJS)" ]; then echo "Error: No .c source files found!"; exit 1; fi
	$(LD) $(GOLDHEN_SDK)/build/crtprx.o $(OBJS) -o $(BUILDDIR)/temp.elf $(LDFLAGS)
	@ls -lh $(BUILDDIR)/temp.elf
	$(CREATE_FSELF) -in=$(BUILDDIR)/temp.elf -out=$(TARGET) --paid 0x3800000000000011
	@if [ ! -s "$(TARGET)" ]; then echo "Error: $(TARGET) is 0 bytes!"; exit 1; fi
	@ls -lh $(TARGET)

$(BUILDDIR)/%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILDDIR):
	@mkdir -p $(BUILDDIR)

clean:
	rm -rf $(BUILDDIR) $(TARGET)
