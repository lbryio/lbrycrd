linux_CFLAGS=-pipe
linux_CXXFLAGS=$(linux_CFLAGS) -std=c++11

linux_release_CFLAGS=-O3 -g
ifeq (1,$(shell ldd --version | head -1 | awk '{print $$NF < 2.28}'))
linux_release_CFLAGS+= -include $(BASEDIR)/glibc_version_header/force_link_glibc_2.19.h
endif
linux_release_CXXFLAGS=$(linux_release_CFLAGS)

linux_debug_CFLAGS=-O1 -g
linux_debug_CXXFLAGS=-O0 -g

linux_debug_CPPFLAGS=-D_GLIBCXX_DEBUG -D_GLIBCXX_DEBUG_PEDANTIC

ifeq (86,$(findstring 86,$(build_arch)))
i686_linux_CC=cc -m32
i686_linux_CXX=c++ -m32
i686_linux_AR=ar
i686_linux_RANLIB=ranlib
i686_linux_NM=nm
i686_linux_STRIP=strip

x86_64_linux_CC=cc -m64
x86_64_linux_CXX=c++ -m64
x86_64_linux_AR=ar
x86_64_linux_RANLIB=ranlib
x86_64_linux_NM=nm
x86_64_linux_STRIP=strip
else
i686_linux_CC=cc -m32
i686_linux_CXX=c++ -m32
x86_64_linux_CC=cc -m64
x86_64_linux_CXX=c++ -m64
endif
