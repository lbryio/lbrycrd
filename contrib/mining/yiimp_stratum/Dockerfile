FROM alpine:3.7
ARG REPOSITORY=https://github.com/tpruvot/yiimp.git

ENV BUILD_DEPS \
 build-base \
 git

ENV RUN_DEPS \
 curl-dev \
 gmp-dev \
 mariadb-dev \ 
 libssh2-dev \
 curl

RUN apk update \
 && apk add --no-cache ${BUILD_DEPS} \   
 && apk add --no-cache ${RUN_DEPS} \   
 && git clone --progress ${REPOSITORY} ~/yiimp \
 && sed -i 's/ulong/uint64_t/g' ~/yiimp/stratum/algos/rainforest.c \
 && find ~/yiimp -name '*akefile' -exec sed -i 's/-march=native//g' {} + \
 && make -C ~/yiimp/stratum/iniparser \
 && make -C ~/yiimp/stratum \
 && mkdir /var/stratum /var/stratum/config \
 && cp ~/yiimp/stratum/run.sh /var/stratum \
 && cp ~/yiimp/stratum/config/run.sh /var/stratum/config \
 && cp ~/yiimp/stratum/stratum /var/stratum \
 && cp ~/yiimp/stratum/config.sample/lbry.conf /var/stratum/config \
 && sed -i 's/yaamp.com/127.0.0.1/g' /var/stratum/config/lbry.conf \
 && sed -i 's/yaampdb/127.0.0.1/g' /var/stratum/config/lbry.conf \
 && rm -rf ~/yiimp \
 && apk del ${BUILD_DEPS} \
 && rm -rf /var/cache/apk/*
 
RUN apk add --no-cache bash

ARG VCS_REF
ARG BUILD_DATE
LABEL maintainer="blockchain@lbry.com" \
    decription="yiimp_stratum" \
    version="1.0" \
    org.label-schema.name="yiimp_stratum" \
    org.label-schema.description="Use this to run yiimp's stratum server in lbry mode" \
    org.label-schema.build-date=$BUILD_DATE \
    org.label-schema.vcs-ref=$VCS_REF \
    org.label-schema.vcs-url="https://github.com/lbryio/lbrycrd" \
    org.label-schema.schema-version="1.0.0-rc1" \
    org.label-schema.vendor="LBRY" \
    org.label-schema.docker.cmd="docker build --build-arg BUILD_DATE=`date -u +"%Y-%m-%dT%H:%M:%SZ"` --build-arg VCS_REF=`git rev-parse --short HEAD` -t lbry/yiimp_stratum yiimp_stratum"

WORKDIR /var/stratum

CMD ["./stratum", "config/lbry"]

EXPOSE 3334
