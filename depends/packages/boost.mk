package=boost
$(package)_version=1_70_0
$(package)_download_path=https://dl.bintray.com/boostorg/release/1.70.0/source/
$(package)_file_name=$(package)_$($(package)_version).tar.bz2
$(package)_sha256_hash=430ae8354789de4fd19ee52f3b1f739e1fba576f0aded0897c3c2bc00fb38778
$(package)_dependencies:icu

define $(package)_set_vars
$(package)_config_opts_release=variant=release
$(package)_config_opts_debug=variant=debug
$(package)_config_opts=--layout=tagged --build-type=complete --user-config=user-config.jam boost.locale.iconv=off boost.locale.posix=off
$(package)_config_opts+=threading=multi link=static -sNO_BZIP2=1 -sNO_ZLIB=1 -sICU_PATH=$(ICU_DIR)
$(package)_config_opts+=-sICU_LINK=-L$(ICU_DIR) -lsicudt -lsicuin -lsicuio -lsicule -lsiculx -lsicutest -lsicutu -lsicuuc
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
$(package)_cxxflags=-std=c++11 -fvisibility=hidden
$(package)_cxxflags_linux=-fPIC
$(package)_config_env+=BOOST_ICU_ICONV="off"
$(package)_config_env+=BOOST_ICU_POSIX="off"
$(package)_config_env+=ICU_PREFIX=$(ICU_DIR)
$(package)_config_env+=BOOST_ICU_LIBS="-L$(ICU_DIR) -lsicudt -lsicuin -lsicuio -lsicule -lsiculx -lsicutest -lsicutu -lsicuuc"
$(package)_build_env+=BOOST_ICU_ICONV="off"
$(package)_build_env+=BOOST_ICU_POSIX="off"
$(package)_build_env+=ICU_PREFIX=$(ICU_DIR)
$(package)_build_env+=BOOST_ICU_LIBS="-L$(ICU_DIR) -lsicudt -lsicuin -lsicuio -lsicule -lsiculx -lsicutest -lsicutu -lsicuuc"
endef

define $(package)_preprocess_cmds
  echo "using $(boost_toolset_$(host_os)) : : $($(package)_cxx) : <cxxflags>\"$($(package)_cxxflags) $($(package)_cppflags)\" <linkflags>\"$($(package)_ldflags)\" <archiver>\"$(boost_archiver_$(host_os))\" <striper>\"$(host_STRIP)\"  <ranlib>\"$(host_RANLIB)\" <rc>\"$(host_WINDRES)\" : ;" > user-config.jam
endef

define $(package)_config_cmds
  echo "int main() { return 0; }" > ./libs/locale/build/has_icu_test.cpp && echo "int main() { return 0; }" > ./libs/regex/build/has_icu_test.cpp && echo "ICU INSTALL: $(ICU_DIR)" && echo "BOOST CONFIG LIBRARIES: $(boost_config_libraries)" && ./bootstrap.sh --with-icu=$(ICU_DIR) --with-libraries=$(boost_config_libraries)
endef

define $(package)_build_cmds
  ICU_PATH=$(ICU_DIR) ./b2 link=static cxxflags=-fPIC -d0 -q -j12 --prefix=$($(package)_staging_prefix_dir) $($(package)_config_opts) install
endef

define $(package)_stage_cmds
endef
