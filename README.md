# LBRYcrd - The LBRY blockchain

[![Build Status](https://travis-ci.org/lbryio/lbrycrd.svg?branch=master)](https://travis-ci.org/lbryio/lbrycrd)
LBRYcrd uses a blockchain similar to bitcoin's to implement an index and payment system for content on the LBRY network. It is a fork of bitcoin core. In addition to the libraries used by bitcoin, LBRYcrd also uses icu4c.

Please read the [lbry.tech overview](https://lbry.tech/overview) for a general understanding of the LBRY pieces. From there you could read the [LBRY spec](https://spec.lbry.com/) for specifics on the data in the blockchain. 

## Installation

Latest binaries are available from https://github.com/lbryio/lbrycrd/releases. There is no installation procedure; the CLI binaries will run as-is and will have any uncommon dependencies statically linked into the binary. The QT GUI is not supported. LBRYcrd is distributed as a collection of executable files; traditional installers are not provided. 

## Usage

The `lbrycrdd` executable will start a LBRYcrd node and connect you to the LBRYcrd network. Use the `lbrycrd-cli` executable
to interact with lbrycrdd through the command line. Command-line help for both executables are available through
the "--help" flag (e.g. `lbrycrdd --help`). Examples:

#### Examples:

Run `./lbrycrdd -server -daemon` to start lbrycrdd in the background.

Run `./lbrycrd-cli -getinfo` to check for some basic information about your LBRYcrd node.

The contribution workflow is described in [CONTRIBUTING.md](CONTRIBUTING.md)
and useful hints for developers can be found in [doc/developer-notes.md](doc/developer-notes.md).

Test locally:
```
./lbrycrdd -server -regtest -txindex  # run this in its own window
./lbrycrd-cli -regtest generate 120   # mine 20 spendable coins
./lbrycrd-cli -regtest claimname my_name deadbeef 1 # hold a name claim with 1 coin
./lbrycrd-cli -regtest generate 1     # get that claim into the block
./lbrycrd-cli -regtest listnameclaims # show owned claims
./lbrycrd-cli -regtest getclaimsforname my_name # show claims under that name
./lbrycrd-cli -regtest stop           # kill lbrycrdd
rm -fr ~/.lbrycrd/regtest/            # destroy regtest data
```
For further understanding of a "regtest" setup, see the local stack setup instructions here: https://lbry.tech/resources/regtest-setup

The CLI help is also browsable online at https://lbry.tech/api/blockchain

#### Data directory:

Lbrycrdd will use the below default data directories (changeable with -datadir):

```
Windows:  %APPDATA%\lbrycrd
Mac:      ~/Library/Application Support/lbrycrd
Unix:     ~/.lbrycrd
```
The data directory contains various things such as your default wallet (wallet.dat), debug logs (debug.log), and blockchain data. You can optionally create a configuration file lbrycrd.conf in the default data directory which will be used by default when running lbrycrdd.

For a list of configuration parameters, run `./lbrycrdd --help`. Below is a sample lbrycrd.conf to enable JSON RPC server on lbrycrdd.

```
rpcuser=lbry
rpcpassword=xyz123456790
daemon=1
server=1
txindex=1
```

## Running from Source
The easiest way to compile is to utilize the Docker image that contains the necessary compilers: lbry/build_lbrycrd. This will allow you to reproduce the build as made on our build servers. I this sample we map a local lbrycrd folder and a local ccache folder inside the image:
```
git clone https://github.com/lbryio/lbrycrd.git
cd lbrycrd
docker run -v "$(pwd):/lbrycrd" --rm -v "${HOME}/ccache:/ccache" -w /lbrycrd -e CCACHE_DIR=/ccache lbry/build_lbrycrd packaging/build_linux_64bit.sh
```
Some examples of compiling directly:
#### Ubuntu with pulled static dependencies:
```
sudo apt install build-essential git libtool autotools-dev automake pkg-config bsdmainutils curl ca-certificates
git clone https://github.com/lbryio/lbrycrd.git
cd lbrycrd
./packaging/build_linux_64bit.sh
./src/test/test_lbrycrd

```
Other Linux distros would be similar. The build shell script is fairly trivial; take a peek at its contents.
#### Ubuntu with local shared dependencies:
Note: using untested dependencies may lead to conflicting results.
```
sudo add-apt-repository ppa:bitcoin/bitcoin
sudo apt-get update
sudo apt-get install libdb4.8-dev libdb4.8++-dev libicu-dev libssl-dev libevent-dev \ 
                     build-essential git libtool autotools-dev automake pkg-config bsdmainutils curl ca-certificates \
                     libboost-system-dev libboost-filesystem-dev libboost-chrono-dev libboost-test-dev libboost-thread-dev
                    
# optionally include libminiupnpc-dev libzmq3-dev

git clone https://github.com/lbryio/lbrycrd.git
cd lbrycrd
./autogen.sh
./configure --enable-static --disable-shared --with-pic --without-gui CXXFLAGS="-O3 -march=native"
make -j$(nproc)
./src/lbrycrdd -server ...

```
#### MacOS (cross-compiled):
```
sudo apt-get install clang llvm git libtool autotools-dev automake pkg-config bsdmainutils curl ca-certificates \
                     libboost-system-dev libboost-filesystem-dev libboost-chrono-dev libboost-test-dev libboost-thread-dev
                    
git clone https://github.com/lbryio/lbrycrd.git
cd lbrycrd
# download MacOS SDK from your favorite source
mkdir depends/SDKs
tar ... extract SDK to depends/SDKs/MacOSX10.11.sdk
./packaging/build_darwin_64bit.sh

```
Look in packaging/build_darwin_64bit.sh for further understanding.
#### MacOS with local shared dependencies:
```
brew install boost berkeley-db@4 icu4c libevent
# fix conflict with gawk pulled first:
brew reinstall readline
brew reinstall gawk

git clone https://github.com/lbryio/lbrycrd.git
cd lbrycrd/depends
make NO_QT=1
cd ..
./autogen.sh
CONFIG_SITE=$(pwd)/depends/x86_64-apple-darwin15.6.0/share/config.site ./configure --enable-static --disable-shared --with-pic --without-gui --enable-reduce-exports CXXFLAGS=-O2
make -j$(sysctl -n hw.ncpu)

```
#### Windows (cross-compiled):
Compiling on MS Windows (outside of WSL) is not supported. The Windows build is cross-compiled from Linux like so:
```
sudo apt-get install build-essential git libtool autotools-dev automake pkg-config bsdmainutils curl ca-certificates \
                     g++-mingw-w64-x86-64 mingw-w64-x86-64-dev
                     
update-alternatives --set x86_64-w64-mingw32-g++ /usr/bin/x86_64-w64-mingw32-g++-posix
                    
git clone https://github.com/lbryio/lbrycrd.git
cd lbrycrd
./packaging/build_windows_64bit.sh

```

If you encounter any errors, please check `doc/build-*.md` for further instructions. If you're still stuck, [create an issue](https://github.com/lbryio/lbrycrd/issues/new) with the output of that command, your system info, and any other information you think might be helpful. The scripts in the packaging folder are simple and will grant extra light on the build process as needed.

#### Use with CLion:
CLion has not traditionally supported Autotools projects, although some progress on that is now in the works. We do include a cmake build file for compiling lbrycrd. See contrib/cmake. Alas, CLion doesn't support external projects in cmake, so that particular approach is also insufficient. CLion does support "compile_commands.json" projects. Fortunately, this can be easily generated for lbrycrd like so:
```
pip install --user compiledb
./autogen.sh && ./configure --enable-static=no --enable-shared --with-pic --without-gui CXXFLAGS="-O0 -g" CFLAGS="-O0 -g" # or whatever normal lbrycrd config
compiledb make -j10
```
Then open the newly generated compile_commands.json file as a project in CLion. Debugging is supported if you compiled with `-g`. To enable that you will need to create a target in CLion by going to File -> Settings -> Build -> Custom Build Targets. Add an empty target with your choice of name. From there you can go to "Edit Configurations", typically found in a drop-down at the top of the editor. Add a Custom Build Application, select your new target, select the compiled file (i.e. test_lbrycrd or lbrycrdd, etc), and then add any necessary command line parameters. Ensure that there is nothing in the "Before launch" section.

## Contributing

Contributions to this project are welcome, encouraged, and compensated. For more details, see [https://lbry.tech/contribute](https://lbry.tech/contribute)

We follow the same coding guidelines as documented by Bitcoin Core, see [here](/doc/developer-notes.md). To run an automated code formatting check, try:
`git diff -U0 master -- '*.h' '*.cpp' | ./contrib/devtools/clang-format-diff.py -p1`. This will check any commits not on master for proper code formatting.
We try to avoid altering parts of the code that is inherited from Bitcoin Core unless absolutely necessary. This will make it easier to merge changes from Bitcoin Core. If commits are expected not to be merged upstream (i.e. we broke up a commit from Bitcoin Core in order to use a single feature in it), the commit message must contain the string "NOT FOR UPSTREAM MERGE".

The `master` branch is regularly built and tested, but is not guaranteed to be
completely stable. [Releases](https://github.com/lbryio/lbrycrd/releases) are created
regularly to indicate new official, stable release versions.

Testing and code review is the bottleneck for development; we get more pull
requests than we can review and test on short notice. Please be patient and help out by testing
other people's pull requests, and remember this is a security-critical project where any mistake might cost people
lots of money. Developers are strongly encouraged to write [unit tests](/doc/unit-tests.md) for new code and to
submit new unit tests for old code. Unit tests are compiled by default and can be run with `src/test/test_lbrycrd`.

The Travis CI system makes sure that every pull request is built, and that unit and sanity tests are automatically run. See https://travis-ci.org/lbryio/lbrycrd

### Testnet

Testnet is maintained for testing purposes and can be accessed using the command `./lbrycrdd -testnet`. If you would like to obtain testnet credits, please contact brannon@lbry.com or grin@lbry.com .

It is easy to solo mine on testnet. (It's easy on mainnet too, but much harder to win.) For instructions see https://github.com/lbryio/sgminer-gm and https://github.com/lbryio/lbrycrd/tree/master/contrib/mining 

## License

This project is MIT licensed. For the full license, see [LICENSE](LICENSE).

## Security

We take security seriously. Please contact security@lbry.com regarding any security issues.
Our PGP key is [here](https://keybase.io/lbry/key.asc) if you need it.

## Contact

The primary contact for this project is [@BrannonKing](https://github.com/BrannonKing) (brannon@lbry.com)


