TOOLCHAIN := $(OO_PS4_TOOLCHAIN)
GOLDHEN_SDK := $(GOLDHEN_PLUGINS_SDK)

PROJDIR   := $(shell pwd)
SOURCEDIR := $(PROJDIR)/source
BUILDDIR  := $(PROJDIR)/build
TARGET    := ps4-timeSync.prx

CC      := clang
LD      := ld.lld
CREATE_FSELF := $(TOOLCHAIN)/bin/linux/create-fself

CFILES  := $(wildcard $(SOURCEDIR)/*.c)
OBJS    := $(patsubst $(SOURCEDIR)/%.c, $(BUILDDIR)/%.o, $(CFILES))

CFLAGS  := --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -I$(TOOLCHAIN)/include -I$(TOOLCHAIN)/include/c++/v1 -I$(GOLDHEN_SDK)/include -O2 -Wall
LDFLAGS := -m elf_x86_64 -pie --script $(TOOLCHAIN)/link.x --eh-frame-hdr -L$(TOOLCHAIN)/lib -L$(GOLDHEN_SDK) -lGoldHEN_Hook -lc -lkernel -lSceNet -lSceSysmodule

all: $(TARGET)

$(TARGET): $(BUILDDIR) $(OBJS)
	$(LD) $(TOOLCHAIN)/lib/crtlib.o $(OBJS) -o $(BUILDDIR)/temp.elf $(LDFLAGS)
	$(CREATE_FSELF) -in=$(BUILDDIR)/temp.elf -out=$(TARGET) --lib=$(TARGET) --paid 0x3800000000000011

$(BUILDDIR)/%.o: $(SOURCEDIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILDDIR):
	@mkdir -p $(BUILDDIR)

clean:
	rm -rf $(BUILDDIR) $(TARGET)
