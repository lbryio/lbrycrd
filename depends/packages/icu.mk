package=icu
$(package)_version=57_1
$(package)_download_path=http://download.icu-project.org/files/icu4c/57.1
$(package)_file_name=$(package)4c-$($(package)_version)-src.tgz
$(package)_sha256_hash=ff8c67cb65949b1e7808f2359f2b80f722697048e90e7cfc382ec1fe229e9581
$(package)_build_subdir=source

define $(package)_set_vars
  $(package)_config_opts=--enable-debug --disable-release --host=x86_64-w64-mingw32 --with-cross-build=/tmp/icu_staging/icu/source --enable-extras=no --enable-strict=no --enable-static --enable-shared=no --enable-tests=no --enable-samples=no --enable-dyload=no
  $(package)_config_opts_release=--disable-debug --enable-release
  $(package)_config_opts_mingw32=--host=x86_64-w64-mingw32
  $(package)_config_opts_linux=--with-pic
endef

define $(package)_config_cmds
  $($(package)_autoconf)
endef

define $(package)_build_cmds
  $(MAKE)
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=/tmp/icu_install install
endef
