TOOLCHAIN := $(OO_PS4_TOOLCHAIN)
JBC_DIR   := $(LIBJBC_DIR)

PROJDIR   := $(shell pwd)
SOURCEDIR := $(PROJDIR)/source
BUILDDIR  := $(PROJDIR)/build
TARGET    := eboot.bin

CC           := clang
LD           := ld.lld
CREATE_FSELF := $(TOOLCHAIN)/bin/linux/create-fself

CFILES := $(wildcard $(SOURCEDIR)/*.c) $(wildcard $(PROJDIR)/*.c) $(wildcard $(JBC_DIR)/*.c)
OBJS   := $(patsubst %.c, $(BUILDDIR)/%.o, $(notdir $(CFILES)))

VPATH  := $(SOURCEDIR):$(PROJDIR):$(JBC_DIR)

CFLAGS  := --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -I$(TOOLCHAIN)/include -I$(JBC_DIR) -O2 -Wall
LDFLAGS := -m elf_x86_64 -pie --script $(TOOLCHAIN)/link.x --eh-frame-hdr -L$(TOOLCHAIN)/lib -lc -lkernel -lSceNet -lSceSysmodule -lSceSystemService

all: $(TARGET)

$(TARGET): $(BUILDDIR) $(OBJS)
	$(LD) $(TOOLCHAIN)/lib/crt1.o $(OBJS) -o $(BUILDDIR)/temp.elf $(LDFLAGS)
	$(CREATE_FSELF) -in=$(BUILDDIR)/temp.elf -eboot=$(TARGET) --paid 0x3800000000000011
	@ls -lh $(TARGET)

$(BUILDDIR)/%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILDDIR):
	@mkdir -p $(BUILDDIR)

clean:
	rm -rf $(BUILDDIR) $(TARGET)
