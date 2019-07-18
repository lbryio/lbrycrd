package=icu
$(package)_version=63_2
$(package)_download_path=https://github.com/unicode-org/icu/releases/download/release-63-2/
$(package)_file_name=$(package)4c-$($(package)_version)-src.tgz
$(package)_sha256_hash=4671e985b5c11252bff3c2374ab84fd73c609f2603bb6eb23b8b154c69ea4215
$(package)_build_subdir=source
$(package)_standard_opts=--disable-extras --disable-strict --enable-static --disable-shared --disable-tests --disable-samples --disable-dyload --disable-layoutex

define $(package)_set_vars
  $(package)_config_opts=$($(package)_standard_opts)
  $(package)_config_opts_debug=--enable-debug --disable-release
  $(package)_config_opts_release=--disable-debug --enable-release
  $(package)_config_opts_mingw32=--with-cross-build="$($(package)_extract_dir)/build"
  $(package)_config_opts_darwin=--with-cross-build="$($(package)_extract_dir)/build" LIBTOOL="$($(package)_libtool)"
  $(package)_archiver_darwin=$($(package)_libtool)
  $(package)_cflags_linux=-fPIC
  $(package)_cppflags_linux=-fPIC
  $(package)_cxxflags=-std=c++11
endef

define $(package)_preprocess_cmds
  PKG_CONFIG_SYSROOT_DIR=/ \
  PKG_CONFIG_LIBDIR=$(host_prefix)/lib/pkgconfig \
  PKG_CONFIG_PATH=$(host_prefix)/share/pkgconfig \
  sed -i.old 's/^GEN_DEPS.cc.*/& $(CXXFLAGS)/' source/config/mh-mingw* && \
  mkdir -p build && cd build && \
  ../source/runConfigureICU Linux $($(package)_standard_opts) CXXFLAGS=-std=c++11 && \
  $(MAKE) && cd ..
endef

define $(package)_config_cmds
  PKG_CONFIG_SYSROOT_DIR=/ \
  PKG_CONFIG_LIBDIR=$(host_prefix)/lib/pkgconfig \
  PKG_CONFIG_PATH=$(host_prefix)/share/pkgconfig \
  $($(package)_autoconf)
endef

define $(package)_build_cmds
  $(MAKE)
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install
endef
