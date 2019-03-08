FROM ubuntu:16.04

ARG VCS_REF
ARG BUILD_DATE

LABEL maintainer="Brannon King" \
    decription="build_lbrycrd" \
    version="1.0" \
    org.label-schema.name="build_lbrycrd" \
    org.label-schema.description="Use this to generate a reproducible build of LBRYcrd" \
    org.label-schema.build-date=$BUILD_DATE \
    org.label-schema.vcs-ref=$VCS_REF \
    org.label-schema.vcs-url="https://github.com/lbryio/lbrycrd" \
    org.label-schema.schema-version="1.0.0-rc1" \
    org.label-schema.docker.cmd="docker build --build-arg BUILD_DATE=`date -u +"%Y-%m-%dT%H:%M:%SZ"` --build-arg VCS_REF=`git rev-parse --short HEAD` -t build_lbrycrd packaging"

RUN set -xe; \
    apt-get update; \
    apt-get install --no-install-recommends -y build-essential libtool autotools-dev automake pkg-config python3 git wget apt-utils \
        librsvg2-bin libtiff-tools cmake imagemagick libcap-dev libz-dev libbz2-dev python-setuptools xz-utils \
        g++-mingw-w64-i686 mingw-w64-i686-dev bsdmainutils curl ca-certificates g++-mingw-w64-x86-64 mingw-w64-x86-64-dev; \
    rm -rf /var/lib/apt/lists/*; \
    update-alternatives --set x86_64-w64-mingw32-g++ /usr/bin/x86_64-w64-mingw32-g++-posix; \
    update-alternatives --set i686-w64-mingw32-g++ /usr/bin/i686-w64-mingw32-g++-posix;

WORKDIR /home
CMD ["/bin/bash"]
