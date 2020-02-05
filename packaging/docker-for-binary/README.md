# lbrycrd Docker image

## Scripts

There are two scripts `deploy.sh` and `auto_deploy.sh` These are used to create 
docker images to push to lbry's Docker Hub.

### `auto_deploy.sh`

This script is used by the LBRYcrd's CI. When a branch or tag is built 
it will create a docker image for it and push it to DockerHub. This should
not be used outside of the CI environment. In addition to this, the 
`Dockerfile.Auto` is associated with this script. This will only run on the
Linux build job. 

#### Requirements

- You would need to build lbrycrd with the Docker image for reproducible 
builds. https://hub.docker.com/repository/docker/lbry/build_lbrycrd
- You will need DockerHub credentials to run this locally. 
- When the script is executed you will need the parameter as
 `./auto_deploy.sh ${tag_name}`

### `deploy.sh`

This can be used locally to manually create a docker image based on a 
release. `release_url` is requested as a parameter to the script when 
run locally. This will grab the binary from github releases inside the
image build. 

#### Requirements

- You will need DockerHub credentials to run this locally. 

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

