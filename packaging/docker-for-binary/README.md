# lbrycrd Docker image
`

## Configuration

The lbrycrd container comes with a default configuration you can use for
production. Extra configuration is optional.

The container includes a `start` script that offers a flexible configuration
style. It allows you to mount your own `lbrycrd.conf` file, or use environment
variables, or a mix of both.

### Environment variables

The environment variables override the values in the mounted config file. If no
mounted config file exists, these variables are used to create a fresh config.

 * `PORT` - The main lbrycrd port
 * `RPC_USER` - The rpc user
 * `RPC_PASSWORD` - The rpc user's password
 * `RPC_ALLOW_IP` - the subnet that is allowed rpc access
 * `RPC_PORT` - The port to bind the rpc service to
 * `RPC_BIND` - The ip address to bind the rpc service to
 

### Example run commands

Running the default configuration:

```
docker run --rm -it -e RUN_MODE=default -e SNAPSHOT_URL="https://lbry.com/snapshot/blockchain" lbry/lbrycrd:latest-release
```

Running with RPC password changed:

```
docker run --rm -it -e RUN_MODE=default -e RPC_PASSWORD=hunter2 lbry/lbrycrd:latest-release
```

Running with a config file but with the RPC password still overridden:

```
docker run --rm -it -v /path/to/lbrycrd.conf:/etc/lbry/lbrycrd.conf -e RUN_MODE=default -e RPC_PASSWORD=hunter2 lbry/lbrycrd:latest-release
```

