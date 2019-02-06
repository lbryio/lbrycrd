package=boost
$(package)_version=1_59_0
$(package)_download_path=http://sourceforge.net/projects/boost/files/boost/1.59.0
$(package)_file_name=$(package)_$($(package)_version).tar.bz2
$(package)_sha256_hash=727a932322d94287b62abb1bd2d41723eec4356a7728909e38adb65ca25241ca
$(package)_dependencies:icu

define $(package)_set_vars
$(package)_config_opts_release=variant=release
$(package)_config_opts_debug=variant=debug
$(package)_config_opts=--layout=tagged --build-type=complete --user-config=user-config.jam boost.locale.iconv=off boost.locale.posix=off
$(package)_config_opts+=threading=multi link=static -sNO_BZIP2=1 -sNO_ZLIB=1 -sICU_PATH=$(shell cat /tmp/icu_install_dir)
$(package)_config_opts+=-sICU_LINK="-L$(shell cat /tmp/icu_install_dir) -lsicudt -lsicuin -lsicuio -lsicule -lsiculx -lsicutest -lsicutu -lsicuuc"
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
$(package)_cxxflags=-fvisibility=hidden
$(package)_cxxflags_linux=-fPIC
$(package)_config_env+=BOOST_ICU_ICONV="off"
$(package)_config_env+=BOOST_ICU_POSIX="off"
$(package)_config_env+=ICU_PREFIX=$(shell cat /tmp/icu_install_dir)
$(package)_config_env+=BOOST_ICU_LIBS="-L$(shell cat /tmp/icu_install_dir) -lsicudt -lsicuin -lsicuio -lsicule -lsiculx -lsicutest -lsicutu -lsicuuc"
$(package)_build_env+=BOOST_ICU_ICONV="off"
$(package)_build_env+=BOOST_ICU_POSIX="off"
$(package)_build_env+=ICU_PREFIX=$(shell cat /tmp/icu_install_dir)
$(package)_build_env+=BOOST_ICU_LIBS="-L$(shell cat /tmp/icu_install_dir) -lsicudt -lsicuin -lsicuio -lsicule -lsiculx -lsicutest -lsicutu -lsicuuc"
endef

define $(package)_preprocess_cmds
  echo "using $(boost_toolset_$(host_os)) : : $($(package)_cxx) : <cxxflags>\"$($(package)_cxxflags) $($(package)_cppflags)\" <linkflags>\"$($(package)_ldflags)\" <archiver>\"$(boost_archiver_$(host_os))\" <striper>\"$(host_STRIP)\"  <ranlib>\"$(host_RANLIB)\" <rc>\"$(host_WINDRES)\" : ;" > user-config.jam
endef

define $(package)_config_cmds
  echo "int main() { return 0; }" > ./libs/locale/build/has_icu_test.cpp && echo "int main() { return 0; }" > ./libs/regex/build/has_icu_test.cpp && ./bootstrap.sh --with-icu=$(shell cat /tmp/icu_install_dir) --with-libraries=$(boost_config_libraries)
endef

define $(package)_build_cmds
  ./b2 link=static cxxflags=-fPIC -d0 -j4 --prefix=$($(package)_staging_prefix_dir) $($(package)_config_opts)
endef

define $(package)_stage_cmds
  ./b2 link=static cxxflags=-fPIC -d0 -j4 --prefix=$($(package)_staging_prefix_dir) $($(package)_config_opts) install
endef
