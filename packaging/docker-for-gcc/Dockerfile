FROM ubuntu:18.04
ENV LANG C.UTF-8

RUN set -xe; \
    apt-get update; \
    apt-get install --no-install-recommends -y build-essential libtool autotools-dev automake pkg-config git wget apt-utils \
        librsvg2-bin cmake libcap-dev libz-dev libbz2-dev python-setuptools python3-setuptools xz-utils ccache \
        bsdmainutils curl ca-certificates; \
    rm -rf /var/lib/apt/lists/*; \
    /usr/sbin/update-ccache-symlinks;
    
ARG VCS_REF
ARG BUILD_DATE
LABEL maintainer="blockchain@lbry.com" \
    decription="build_lbrycrd_gcc" \
    version="1.2" \
    org.label-schema.name="build_lbrycrd_gcc" \
    org.label-schema.description="Use this to generate a reproducible build of LBRYcrd for Linux" \
    org.label-schema.build-date=$BUILD_DATE \
    org.label-schema.vcs-ref=$VCS_REF \
    org.label-schema.vcs-url="https://github.com/lbryio/lbrycrd" \
    org.label-schema.schema-version="1.0.0-rc1" \
    org.label-schema.vendor="LBRY" \
    org.label-schema.docker.cmd="docker build --build-arg BUILD_DATE=`date -u +"%Y-%m-%dT%H:%M:%SZ"` --build-arg VCS_REF=`git rev-parse --short HEAD` -t lbry/build_lbrycrd_gcc packaging"

ENV PATH "/usr/lib/ccache:$PATH"
WORKDIR /home
CMD ["/bin/bash"]
