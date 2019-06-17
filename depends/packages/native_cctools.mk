package=native_cctools
$(package)_version=807d6fd1be5d2224872e381870c0a75387fe05e6
$(package)_download_path=https://github.com/theuni/cctools-port/archive
$(package)_file_name=$($(package)_version).tar.gz
$(package)_sha256_hash=a09c9ba4684670a0375e42d9d67e7f12c1f62581a27f28f7c825d6d7032ccc6a
$(package)_build_subdir=cctools

define $(package)_set_vars
  $(package)_config_opts=--target=$(host) --disable-lto-support --prefix=/
  $(package)_ldflags+=-Wl,-rpath=\\$$$$$$$$\$$$$$$$$ORIGIN/../lib
  $(package)_cc=cc
  $(package)_cxx=c++
endef

define $(package)_preprocess_cmds
  cd $($(package)_build_subdir); ./autogen.sh && \
  sed -i.old "/define HAVE_PTHREADS/d" ld64/src/ld/InputFiles.h
endef

define $(package)_config_cmds
  $($(package)_autoconf)
endef

define $(package)_build_cmds
  $(MAKE)
endef

define $(package)_stage_cmds
  mkdir -p $($(package)_staging_prefix_dir) && \
  $(MAKE) DESTDIR=$($(package)_staging_prefix_dir) install
endef
