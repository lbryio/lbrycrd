#!/usr/bin/env bash

CONFIG_PATH=/etc/lbry/lbrycrd.conf


function override_config_option() {
    # Remove existing config line from a config file
    #  and replace with environment fed value.
    # Does nothing if the variable does not exist.
    #  var     Name of ENV variable
    #  option  Name of config option
    #  config  Path of config file
    local var=$1 option=$2 config=$3
    if [[ -v $var ]]; then
        # Remove the existing config option:
        sed -i "/^$option\W*=/d" $config
        # Add the value from the environment:
        echo "$option=${!var}" >> $config
    fi
}


function set_config() {
  if [ -d "$CONFIG_PATH" ]; then
      echo "$CONFIG_PATH is a directory when it should be a file."
      exit 1
  elif [ -f "$CONFIG_PATH" ]; then
      echo "Merging the mounted config file with environment variables."
      local MERGED_CONFIG=/tmp/lbrycrd_merged.conf
      cp $CONFIG_PATH $MERGED_CONFIG
      echo "" >> $MERGED_CONFIG
      override_config_option PORT port $MERGED_CONFIG
      override_config_option RPC_USER rpcuser $MERGED_CONFIG
      override_config_option RPC_PASSWORD rpcpassword $MERGED_CONFIG
      override_config_option RPC_ALLOW_IP rpcallowip $MERGED_CONFIG
      override_config_option RPC_PORT rpcport $MERGED_CONFIG
      override_config_option RPC_BIND rpcbind $MERGED_CONFIG
      override_config_option TX_INDEX txindex $MERGED_CONFIG
      override_config_option MAX_TX_FEE maxtxfee $MERGED_CONFIG
      override_config_option DUST_RELAY_FEE dustrelayfee $MERGED_CONFIG
      # Make the new merged config file the new CONFIG_PATH
      # This ensures that the original file the user mounted remains unmodified
      CONFIG_PATH=$MERGED_CONFIG
  else
      echo "Creating a fresh config file from environment variables."
      cat << EOF > $CONFIG_PATH
server=1
printtoconsole=1

port=${PORT:-9246}
rpcuser=${RPC_USER:-lbry}
rpcpassword=${RPC_PASSWORD:-lbry}
## Be careful what you set this to when running mainnet. By default it only allows RPC connections from localhost
## if running inside a composition of services Then pass the environment variable `RPC_ALLOW_IP=0.0.0.0/0`
rpcallowip=${RPC_ALLOW_IP:-127.0.0.1}
rpcport=${RPC_PORT:-9245}
rpcbind=${RPC_BIND:-"0.0.0.0"}

maxtxfee=${MAX_TX_FEE:-"0.5"}
dustrelayfee=${DUST_RELAY_FEE:-"0.00000001"}
EOF
  fi
  echo "Config: "
  cat $CONFIG_PATH
}


function download_snapshot() {
  local url="${SNAPSHOT_URL:-}" #off by default. latest snapshot at https://lbry.com/snapshot/blockchain
  if [[ -n "$url" ]] && [[ ! -d ./.lbrycrd/blocks ]]; then
    echo "Downloading blockchain snapshot from $url"
    wget --no-verbose -O snapshot.tar.bz2 "$url"
    echo "Extracting snapshot..."
    mkdir -p ./.lbrycrd
    tar xvjf snapshot.tar.bz2 --directory ./.lbrycrd
    rm snapshot.tar.bz2
  fi
}


## Ensure perms are correct prior to running main binary
/usr/bin/fix-permissions


## You can optionally specify a run mode if you want to use lbry defined presets for compatibility.
case $RUN_MODE in
  default )
    set_config
    download_snapshot
    exec lbrycrdd -conf=$CONFIG_PATH
    ;;
  ## If for some reason one is told to reindex their blockchain this run mode will launch lbrycrd with the reindex parameter.
  ## Don't forget to change it back to default once complete ( you will need to remove the container to re-apply the run mode).
  reindex )
    ## Apply this RUN_MODE in the case you need to update a dataset.  NOTE: you do not need to use `RUN_MODE reindex` for more than one complete run.
    set_config
    exec lbrycrdd -conf=$CONFIG_PATH -reindex
    ;;
  ## Regtest requires ports to be the port listed below. It is hardcoded to be this port for regtest when using a config.
  ## Only way to override it is to run lbrycrd from the commandline and set the port there.
  regtest )
    ## Set config params
    ## TODO: Make this more automagic in the future.
    mkdir -p `dirname $CONFIG_PATH`
    echo "rpcuser=lbry" >           $CONFIG_PATH
    echo "rpcpassword=lbry" >>      $CONFIG_PATH
    echo "rpcport=29245" >>         $CONFIG_PATH
    echo "rpcbind=0.0.0.0" >>       $CONFIG_PATH
    echo "rpcallowip=0.0.0.0/0" >>  $CONFIG_PATH
    echo "regtest=1" >>             $CONFIG_PATH
    echo "server=1" >>              $CONFIG_PATH
    echo "printtoconsole=1" >>      $CONFIG_PATH

    [ "${AUTO_ADVANCE:-0}" == "1" ] && nohup advance &>/dev/null &

    exec lbrycrdd -conf=$CONFIG_PATH $1
    ;;
  testnet )
    ## Set config params
    ## TODO: Make this more automagic in the future.
    mkdir -p `dirname $CONFIG_PATH`
    echo "rpcuser=lbry" >           $CONFIG_PATH
    echo "rpcpassword=lbry" >>      $CONFIG_PATH
    echo "rpcport=29245" >>         $CONFIG_PATH
    echo "rpcbind=0.0.0.0" >>       $CONFIG_PATH
    echo "rpcallowip=0.0.0.0/0" >>  $CONFIG_PATH
    echo "testnet=1" >>             $CONFIG_PATH
    echo "server=1" >>              $CONFIG_PATH
    echo "printtoconsole=1" >>      $CONFIG_PATH

    #nohup advance &>/dev/null &
    exec lbrycrdd -conf=$CONFIG_PATH $1
    ;;
  * )
    echo "Error, you must define a RUN_MODE environment variable."
    echo "Available options are testnet, regtest, chainquery, default, and reindex"
    ;;
esac
