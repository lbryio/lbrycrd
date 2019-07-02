FROM ubuntu:16.04
ENV LANG C.UTF-8

RUN set -xe; \
    apt-get update; \
    apt-get install --no-install-recommends -y build-essential libtool autotools-dev automake pkg-config git wget apt-utils \
        librsvg2-bin libtiff-tools cmake imagemagick libcap-dev libz-dev libbz2-dev python-setuptools xz-utils ccache g++-multilib \
        g++-mingw-w64-i686 mingw-w64-i686-dev bsdmainutils curl ca-certificates g++-mingw-w64-x86-64 mingw-w64-x86-64-dev; \
    rm -rf /var/lib/apt/lists/*;

RUN wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key | apt-key add -; \
    echo 'deb http://apt.llvm.org/xenial/ llvm-toolchain-xenial-8 main' >> /etc/apt/sources.list; \
    apt-get update; \
    apt-get install --no-install-recommends -y clang-8 lldb-8 lld-8 libc++-8-dev; \
    rm -rf /var/lib/apt/lists/*;

RUN update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang-cpp-8 80; \
    update-alternatives --install /usr/bin/clang clang /usr/bin/clang-8 80; \
    update-alternatives --install /usr/bin/c++ c++ /usr/bin/clang++ 80; \
    update-alternatives --install /usr/bin/cc cc /usr/bin/clang 80; \
    update-alternatives --set x86_64-w64-mingw32-g++ /usr/bin/x86_64-w64-mingw32-g++-posix; \
    update-alternatives --set i686-w64-mingw32-g++ /usr/bin/i686-w64-mingw32-g++-posix; \
    /usr/sbin/update-ccache-symlinks; \
    cd /usr/include/c++ && ln -s /usr/lib/llvm-8/include/c++/v1; \
    cd /usr/lib/llvm-8/lib && ln -s libc++abi.so.1 libc++abi.so;

ARG VCS_REF
ARG BUILD_DATE
LABEL maintainer="blockchain@lbry.com" \
    decription="build_lbrycrd" \
    version="1.1" \
    org.label-schema.name="build_lbrycrd" \
    org.label-schema.description="Use this to generate a reproducible build of LBRYcrd" \
    org.label-schema.build-date=$BUILD_DATE \
    org.label-schema.vcs-ref=$VCS_REF \
    org.label-schema.vcs-url="https://github.com/lbryio/lbrycrd" \
    org.label-schema.schema-version="1.0.0-rc1" \
    org.label-schema.vendor="LBRY" \
    org.label-schema.docker.cmd="docker build --build-arg BUILD_DATE=`date -u +"%Y-%m-%dT%H:%M:%SZ"` --build-arg VCS_REF=`git rev-parse --short HEAD` -t lbry/build_lbrycrd packaging"

ENV PATH "/usr/lib/ccache:$PATH"
WORKDIR /home
CMD ["/bin/bash"]
