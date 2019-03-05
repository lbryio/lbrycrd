package=icu
$(package)_version=62_1
$(package)_download_path=http://download.icu-project.org/files/icu4c/62.1
$(package)_file_name=$(package)4c-$($(package)_version)-src.tgz
$(package)_sha256_hash=3dd9868d666350dda66a6e305eecde9d479fb70b30d5b55d78a1deffb97d5aa3
$(package)_build_subdir=source
$(package)_standard_opts=--disable-extras --disable-strict --enable-static --disable-shared --disable-tests --disable-samples --disable-dyload --disable-layoutex

define $(package)_set_vars
  $(package)_config_opts=$($(package)_standard_opts)
  $(package)_config_opts_debug=--enable-debug --disable-release
  $(package)_config_opts_release=--disable-debug --enable-release
  $(package)_config_opts_mingw32=--with-cross-build="`pwd`/../build"
  $(package)_config_opts_linux=--with-pic
  $(package)_config_opts_darwin=--with-cross-build="`pwd`/../build" LIBTOOL="$($(package)_libtool)"
  $(package)_archiver_darwin=$($(package)_libtool)
endef

define $(package)_config_cmds
  PKG_CONFIG_SYSROOT_DIR=/ \
  PKG_CONFIG_LIBDIR=$(host_prefix)/lib/pkgconfig \
  PKG_CONFIG_PATH=$(host_prefix)/share/pkgconfig \
  mkdir -p ../build && cd ../build && \
  PATH=$(PATH):$(host_prefix)/native/bin ../source/runConfigureICU `uname` $($(package)_standard_opts) --with-pic && \
  make -j`nproc` && cd ../source && \
 PATH=$(PATH):$(host_prefix)/native/bin  $($(package)_autoconf)
endef

define $(package)_build_cmds
  $(MAKE)
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install
endef
