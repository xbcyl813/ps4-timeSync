# ==============================================================================
# PS4 TimeSync - Standalone PKG Makefile (OpenOrbis PS4 Toolchain)
# ==============================================================================

TOOLCHAIN  := $(OO_PS4_TOOLCHAIN)
PROJDIR    := $(CURDIR)
SRCDIR     := $(PROJDIR)/source
BUILDDIR   := $(PROJDIR)/build
SYSDIR     := $(PROJDIR)/sce_sys
PKGDIR     := $(PROJDIR)/pkg_staging

# PKG 应用信息配置
TITLE_ID   := TIME00001
CONTENT_ID := IV0000-$(TITLE_ID)_00-PS4TIMESYNC00001
TITLE_NAME := PS4 TimeSync
VERSION    := 01.00

TARGET_ELF := $(BUILDDIR)/eboot.elf
TARGET_BIN := $(BUILDDIR)/eboot.bin
TARGET_PKG := $(PROJDIR)/ps4-timeSync.pkg

# 工具链命令定义
CC           := clang
LD           := ld.lld
CREATE_FSELF := $(TOOLCHAIN)/bin/linux/create-fself
PKG_TOOL     := DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1 $(TOOLCHAIN)/bin/linux/PkgTool.Core

# 编译与链接参数
CFLAGS  := --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables \
           -I$(TOOLCHAIN)/include -I$(TOOLCHAIN)/include/c++/v1 -O2 -Wall
LDFLAGS := -m elf_x86_64 -pie --script $(TOOLCHAIN)/link.x --eh-frame-hdr \
           -L$(TOOLCHAIN)/lib -lc -lkernel -lSceNet -lSceSysmodule -lSceRtc -lSceSystemService
CRT1    := $(TOOLCHAIN)/lib/crt1.o

SRCS := $(wildcard $(SRCDIR)/*.c)
OBJS := $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(SRCS))

.PHONY: all clean

all: $(TARGET_PKG)

# 1. 编译 C 源码
$(BUILDDIR)/%.o: $(SRCDIR)/%.c
	@mkdir -p $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# 2. 链接生成 ELF
$(TARGET_ELF): $(OBJS)
	$(LD) $(OBJS) -o $@ $(LDFLAGS) $(CRT1)

# 3. 转换为高权限 FSELF (eboot.bin)
$(TARGET_BIN): $(TARGET_ELF)
	$(CREATE_FSELF) -in=$< -eboot=$@ --paid 0x3800000000000011

# 4. 生成系统元数据 param.sfo
$(SYSDIR)/param.sfo:
	@mkdir -p $(SYSDIR)
	$(PKG_TOOL) sfo_new $@
	$(PKG_TOOL) sfo_setentry $@ APP_TYPE --type Integer --maxsize 4 --value 1
	$(PKG_TOOL) sfo_setentry $@ APP_VER --type Utf8 --maxsize 8 --value "$(VERSION)"
	$(PKG_TOOL) sfo_setentry $@ ATTRIBUTE --type Integer --maxsize 4 --value 0
	$(PKG_TOOL) sfo_setentry $@ CATEGORY --type Utf8 --maxsize 4 --value "gd"
	$(PKG_TOOL) sfo_setentry $@ CONTENT_ID --type Utf8 --maxsize 48 --value "$(CONTENT_ID)"
	$(PKG_TOOL) sfo_setentry $@ DOWNLOAD_DATA_SIZE --type Integer --maxsize 4 --value 0
	$(PKG_TOOL) sfo_setentry $@ SYSTEM_VER --type Integer --maxsize 4 --value 0
	$(PKG_TOOL) sfo_setentry $@ TITLE --type Utf8 --maxsize 128 --value "$(TITLE_NAME)"
	$(PKG_TOOL) sfo_setentry $@ TITLE_ID --type Utf8 --maxsize 12 --value "$(TITLE_ID)"
	$(PKG_TOOL) sfo_setentry $@ VERSION --type Utf8 --maxsize 8 --value "$(VERSION)"

# 5. 生成 GP4 项目文件并打包为最终 PKG（自动包含自定义 icon0.png）
$(TARGET_PKG): $(TARGET_BIN) $(SYSDIR)/param.sfo
	@if [ ! -f "$(SYSDIR)/icon0.png" ]; then \
		echo "错误: 找不到自定义图标 $(SYSDIR)/icon0.png，请先放入 512x512 的 PNG 图片！"; \
		exit 1; \
	fi
	rm -rf $(PKGDIR) $(BUILDDIR)/pkg.gp4
	mkdir -p $(PKGDIR)/sce_sys
	cp $(TARGET_BIN) $(PKGDIR)/eboot.bin
	cp $(SYSDIR)/param.sfo $(PKGDIR)/sce_sys/param.sfo
	cp $(SYSDIR)/icon0.png $(PKGDIR)/sce_sys/icon0.png
	@printf '%s\n' \
		'<?xml version="1.0" encoding="utf-8" standalone="yes"?>' \
		'<psproject fmt="gp4" version="1000">' \
		'  <volume>' \
		'    <volume_type>pkg_ps4_app</volume_type>' \
		'    <volume_id>PS4VOLUME</volume_id>' \
		'    <volume_ts>2026-01-01 00:00:00</volume_ts>' \
		'    <package content_id="$(CONTENT_ID)" passcode="00000000000000000000000000000000" storage_type="digital50" app_type="full"/>' \
		'    <chunk_info chunk_count="1" scenario_count="1">' \
		'      <chunks>' \
		'        <chunk id="0" layer_no="0" label="Chunk #0"/>' \
		'      </chunks>' \
		'      <scenarios default_id="0">' \
		'        <scenario id="0" type="sp" initial_chunk_count="1" label="Scenario #0">0</scenario>' \
		'      </scenarios>' \
		'    </chunk_info>' \
		'  </volume>' \
		'  <files img_no="0">' \
		'    <file targ_path="eboot.bin" orig_path="$(PKGDIR)/eboot.bin"/>' \
		'    <file targ_path="sce_sys/param.sfo" orig_path="$(PKGDIR)/sce_sys/param.sfo"/>' \
		'    <file targ_path="sce_sys/icon0.png" orig_path="$(PKGDIR)/sce_sys/icon0.png"/>' \
		'  </files>' \
		'  <rootdir>' \
		'    <dir targ_name="sce_sys"/>' \
		'  </rootdir>' \
		'</psproject>' > $(BUILDDIR)/pkg.gp4
	$(PKG_TOOL) pkg_build $(BUILDDIR)/pkg.gp4 .
	mv $(CONTENT_ID).pkg $(TARGET_PKG)

clean:
	rm -rf $(BUILDDIR) $(PKGDIR) $(SYSDIR)/param.sfo $(TARGET_PKG)
