# 生成 GP4 项目文件并打包为最终 PKG（自动包含自定义 icon0.png）
$(TARGET_PKG): $(BUILDDIR)/eboot.bin $(SYSDIR)/param.sfo
	@if [ ! -f "$(SYSDIR)/icon0.png" ]; then \
		echo "错误: 找不到自定义图标 $(SYSDIR)/icon0.png，请先放入 512x512 的 PNG 图片！"; \
		exit 1; \
	fi
	rm -rf $(PKGDIR) $(BUILDDIR)/pkg.gp4
	mkdir -p $(PKGDIR)/sce_sys
	cp $(BUILDDIR)/eboot.bin $(PKGDIR)/eboot.bin
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
