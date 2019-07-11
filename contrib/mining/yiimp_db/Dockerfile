FROM mariadb:10.1-bionic
ARG REPOSITORY=https://github.com/tpruvot/yiimp.git

ENV BUILD_DEPS \
 ca-certificates \
 git

COPY init-db.sh /docker-entrypoint-initdb.d/

RUN apt-get update \
 && apt-get install -y --no-install-recommends ${BUILD_DEPS} \
 && git clone --progress ${REPOSITORY} ~/yiimp \
 && mkdir /tmp/sql \
 && mv ~/yiimp/sql/2016-04-03-yaamp.sql.gz /tmp/sql/0000-00-00-initial.sql.gz \
 && cp ~/yiimp/sql/*.sql /tmp/sql \
 && apt-get purge -y --auto-remove ${BUILD_DEPS} \
 && rm -rf /var/lib/apt/lists/* \
 && rm -rf ~/yiimp
  
EXPOSE 3306

ARG VCS_REF
ARG BUILD_DATE
LABEL maintainer="blockchain@lbry.com" \
    decription="yiimp_db" \
    version="1.0" \
    org.label-schema.name="yiimp_db" \
    org.label-schema.description="Use this to run a compatible MariaDB for yiimp's stratum server" \
    org.label-schema.build-date=$BUILD_DATE \
    org.label-schema.vcs-ref=$VCS_REF \
    org.label-schema.vcs-url="https://github.com/lbryio/lbrycrd" \
    org.label-schema.schema-version="1.0.0-rc1" \
    org.label-schema.vendor="LBRY" \
    org.label-schema.docker.cmd="docker build --build-arg BUILD_DATE=`date -u +"%Y-%m-%dT%H:%M:%SZ"` --build-arg VCS_REF=`git rev-parse --short HEAD` -t lbry/yiimp_db yiimp_db"
