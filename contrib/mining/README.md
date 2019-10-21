## Stratum Server Instructions
In simple terms, the stratum protocol is a protocol to distribute crypto mining work to multiple miners. Mining pools typically run a stratum endpoint that the various miners communicate with.
Please refer to other web sources for more information about mining pools or the stratum protocol.

When mining LBC, you can solo mine directly to an instance of a full node (using the node's wallet). Or you can mine as part of a pool. 
You can host your own pool or use one of the many hosted LBC pools. See https://miningpoolstats.stream/lbry

This document refers to Yiimp, a derivative of Yaamp, as found here: https://github.com/tpruvot/yiimp.git .
Please refer to the instructions there as well. Yiimp has supported LBRY mining for several years.

Yiimp consists of two pieces: the web GUI for pool management (written in PHP) and the Stratum server (written in C++). The two communicate via polling a MySQL database (or MariaDB).
The web GUI and configuration of the pooling rewards, fees, etc. are out of scope here.

To help you with running Yiimp, we have created two docker images: one for the DB and one for the Yiimp Stratum Server. (See the subfolders.)
Use of the Docker images is optional; you can refer to other Yiimp and MySQL documentation for running it without Docker.
If you are using your own database instance, you will need to import the Yiimp SQL files to establish the yaamp database.
See https://github.com/tpruvot/yiimp/tree/next/sql .

### Sample Usage Steps:

#### 1. Run the full lbrycrd node:
```
./lbrycrdd -testnet -rpcuser=ruser -rpcpassword=rpswd -deprecatedrpc=validateaddress -deprecatedrpc=accounts -daemon
```
The included deprecated RPCs are required for compatibility with Yiimp.
It will need to be caught up to the current block before it is ready.
Remove `-testnet` for the real deal.
#### 2. Run and initialize the datatabase:
```
docker run -d -e MYSQL_ROOT_PASSWORD=patofpaq -e MYSQL_DATABASE=yaamp --network host --name db lbry/yiimp_db

docker exec -it db mysql -uroot -ppatofpaq
use yaamp;
delete from coins;
insert into coins(name, symbol, symbol2, algo, enable, auto_ready, rpcuser, rpcpasswd, rpchost, rpcport, rpccurl, rpcencoding, hasgetinfo, hassubmitblock, usememorypool, usesegwit, auxpow) 
  values('Local LBRY Instance', 'LBC', 'LBC', 'lbry', 1, 1, 'ruser', 'rpswd', '127.0.0.1', 19245, 1, 'utf-8', 0, 1, 0, 0, 0);
exit
```
Use port 19245 for testnet, port 9245 for main. Set usesegwit to 1 after the segwit fork is enabled on December 11, 2019.
#### 3. Run the stratum server:
```
docker run --network host -d lbry/yiimp_stratum
```
Alternatively, to get more output or see how its called directly:
```
docker run --network host -it lbry/yiimp_stratum bash
cat config/lbry.conf
./stratum config/lbry
```
When testing with an ASIC you may need to modify the TCP server address in said lbry.conf file to be an external IP address.

#### 4. Connect sgminer to it:
```
sgminer -k lbry -o stratum+tcp://127.0.0.1:3334/ -D -T -O mn824Su1wX7ip8WcNYzXwwWqvBvkeWGRo6:x
```
The username there is the account to receive payments from the pool. The password is unused. Tested with https://github.com/lbryio/sgminer-gm.
You can use whatever miner you prefer.
