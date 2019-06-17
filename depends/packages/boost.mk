package=boost
$(package)_version=1_70_0
$(package)_download_path=https://dl.bintray.com/boostorg/release/1.70.0/source/
$(package)_file_name=$(package)_$($(package)_version).tar.bz2
$(package)_sha256_hash=430ae8354789de4fd19ee52f3b1f739e1fba576f0aded0897c3c2bc00fb38778
$(package)_dependencies=icu

define $(package)_set_vars
$(package)_config_opts_release=variant=release
$(package)_config_opts_debug=variant=debug
$(package)_config_opts=--layout=tagged --build-type=complete --user-config=user-config.jam
$(package)_config_opts+=threading=multi link=static -sNO_BZIP2=1 -sNO_ZLIB=1
$(package)_config_opts+=boost.locale.iconv=off boost.locale.posix=off boost.locale.icu=on boost.locale.std=off -sICU_PATH="$(host_prefix)"
# The stupid ICU_LINK handling reorders the dependencies alphabetically, thus making it impossible to get the link order correct.
# To work around this we're using the ldflags below but we need ICU_LINK to be non-blank so that we don't get an auto-generated conflict with ldflags.
$(package)_config_opts+=-sICU_LINK="-time"
$(package)_config_opts_linux=threadapi=pthread runtime-link=shared
$(package)_config_opts_darwin=--toolset=darwin-4.2.1 runtime-link=shared
$(package)_config_opts_mingw32=binary-format=pe target-os=windows threadapi=win32 runtime-link=static
$(package)_config_opts_x86_64_mingw32=address-model=64
$(package)_config_opts_i686_mingw32=address-model=32
$(package)_config_opts_i686_linux=address-model=32 architecture=x86
$(package)_toolset_$(host_os)=gcc
$(package)_archiver_$(host_os)=$($(package)_ar)
$(package)_toolset_darwin=darwin
$(package)_archiver_darwin=$($(package)_libtool)
$(package)_config_libraries=chrono,filesystem,program_options,system,locale,regex,thread,test
$(package)_cxxflags=-std=c++11 -fvisibility=hidden -Wno-deprecated
$(package)_cxxflags_linux=-fPIC
# The ideal doesn't work because vars are evaluated before any dependency is processed:
# $(package)_ldflags=$$(shell PKG_CONFIG_SYSROOT_DIR=/ PKG_CONFIG_LIBDIR=$(host_prefix)/lib/pkgconfig PKG_CONFIG_PATH=$(host_prefix)/share/pkgconfig pkg-config icu-io icu-uc icu-i18n --libs)
# So we substitute poorly (as these may not actually match all scenarios):
$(package)_ldflags_mingw32=-L$(host_prefix)/lib -lsicuio -lsicuuc -lsicudt
$(package)_ldflags_linux=-L$(host_prefix)/lib -licuio -licuuc -licudata -licui18n
$(package)_ldflags_darwin=-L$(host_prefix)/lib -licuio -licuuc -licudata -licui18n
endef

define $(package)_preprocess_cmds
  echo "using $(boost_toolset_$(host_os)) : : $($(package)_cxx) : <cxxflags>\"$($(package)_cxxflags) $($(package)_cppflags)\" <linkflags>\"$($(package)_ldflags)\" <archiver>\"$(boost_archiver_$(host_os))\" <striper>\"$(host_STRIP)\"  <ranlib>\"$(host_RANLIB)\" <rc>\"$(host_WINDRES)\" : ;" > user-config.jam
endef

define $(package)_config_cmds
  ./bootstrap.sh --with-icu="$(host_prefix)" --with-libraries="$(boost_config_libraries)"
endef

define $(package)_build_cmds
  ./b2 -d2 -j`getconf _NPROCESSORS_ONLN` -d1 --reconfigure --prefix=$($(package)_staging_prefix_dir) $($(package)_config_opts) stage
endef

define $(package)_stage_cmds
  ./b2 -d0 -j`getconf _NPROCESSORS_ONLN` --prefix=$($(package)_staging_prefix_dir) $($(package)_config_opts) install
endef
