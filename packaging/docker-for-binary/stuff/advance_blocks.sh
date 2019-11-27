#!/usr/bin/env bash
while sleep 2; do
    lbrycrd-cli -conf=/etc/lbry/lbrycrd.conf generate 1
done
