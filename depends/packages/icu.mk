package=icu
$(package)_version=55_1
$(package)_download_path=http://download.icu-project.org/files/icu4c/55.1
$(package)_file_name=$(package)4c-$($(package)_version)-src.tgz
$(package)_sha256_hash=e16b22cbefdd354bec114541f7849a12f8fc2015320ca5282ee4fd787571457b
$(package)_build_subdir=source

define $(package)_set_vars
  $(package)_config_opts=--enable-debug --disable-release --host=i686-w64-mingw32 --with-cross-build=/tmp/icu_staging/icu/source --enable-extras=no --enable-strict=no -enable-static --enable-shared=no --enable-tests=no --enable-samples=no --enable-dyload=no
  $(package)_config_opts_release=--disable-debug --enable-release
  $(package)_config_opts_mingw32=--host=i686-w64-mingw32
  $(package)_config_opts_linux=--with-pic
endef

define $(package)_config_cmds
  $($(package)_autoconf)
endef

define $(package)_build_cmds
  $(MAKE)
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install
endef

define $(package)_postprocess_cmds
endef
