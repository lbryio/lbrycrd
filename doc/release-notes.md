0.19.1 Release Notes
===============================

Bitcoin Core version 0.19.1 is now available from:

  <https://bitcoincore.org/bin/bitcoin-core-0.19.1/>
This minor release includes various bug fixes and performance
improvements, as well as updated translations.
This is a new major version release, including new features, various bugfixes
and performance improvements, as well as updated translations.

Please report bugs using the issue tracker at GitHub:

  <https://github.com/bitcoin/bitcoin/issues>

To receive security and update notifications, please subscribe to:

  <https://bitcoincore.org/en/list/announcements/join/>

Upgrading directly from a version of Bitcoin Core that has reached its EOL is
possible, but it might take some time if the datadir needs to be migrated. Old
wallet versions of Bitcoin Core are generally supported.
Bitcoin Core is supported and extensively tested on operating systems using
the Linux kernel, macOS 10.10+, and Windows 7 and newer. It is not recommended
to use Bitcoin Core on unsupported systems.
as frequently tested on them.
From Bitcoin Core 0.17.0 onwards, macOS versions earlier than 10.10 are no
longer supported, as Bitcoin Core is now built using Qt 5.9.x which requires
macOS 10.10+. Additionally, Bitcoin Core does not yet change appearance when
macOS "dark mode" is activated.

In addition to previously supported CPU platforms, this release's pre-compiled
distribution provides binaries for the RISC-V platform.
0.19.1 change log
shut down (which might take a few minutes for older versions), then run the
installer (on Windows) or just copy over `/Applications/Bitcoin-Qt` (on Mac)
or `bitcoind`/`bitcoin-qt` (on Linux).
If your node has a txindex, the txindex db will be migrated the first time you run 0.17.0 or newer, which may take up to a few hours. Your node will not be functional until this migration completes.

The first time you run version 0.15.0 or newer, your chainstate database will be converted to a
new format, which will take anywhere from a few minutes to half an hour,
depending on the speed of your machine.

Note that the block database format also changed in version 0.8.0 and there is no
automatic upgrade code from before version 0.8 to version 0.15.0. Upgrading
directly from 0.7.x and earlier without redownloading the blockchain is not supported.
However, as usual, old wallet versions are still supported.

Downgrading warning
-------------------

The chainstate database for this release is not compatible with previous
releases, so if you run 0.15 and then decide to switch back to any
older version, you will need to run the old release with the `-reindex-chainstate`
option to rebuild the chainstate data structures in the old format.

If your node has pruning enabled, this will entail re-downloading and
processing the entire blockchain.

Compatibility
==============

Bitcoin Core is extensively tested on multiple operating systems using
the Linux kernel, macOS 10.10+, and Windows 7 and newer (Windows XP is not supported).

Bitcoin Core should also work on most other Unix-like systems but is not
frequently tested on them.

From 0.17.0 onwards macOS <10.10 is no longer supported. 0.17.0 is built using Qt 5.9.x, which doesn't
support versions of macOS older than 10.10.

- #17643 Fix origfee return for bumpfee with feerate arg (instagibbs)
- #16963 Fix `unique_ptr` usage in boost::signals2 (promag)
- #17258 Fix issue with conflicted mempool tx in listsinceblock (adamjonas, mchrostowski)
- #17924 Bug: IsUsedDestination shouldn't use key id as script id for ScriptHash (instagibbs)
- #17621 IsUsedDestination should count any known single-key address (instagibbs)
- #17843 Reset reused transactions cache (fjahr)
- #17687 cli: Fix fatal leveldb error when specifying -blockfilterindex=basic twice (brakmic)
- #17728 require second argument only for scantxoutset start action (achow101)
- #17445 zmq: Fix due to invalid argument and multiple notifiers (promag)
- #17524 psbt: handle unspendable psbts (achow101)
- #17156 psbt: check that various indexes and amounts are within bounds (achow101)
- #17427 Fix missing qRegisterMetaType for `size_t` (hebasto)
- #17695 disable File-\>CreateWallet during startup (fanquake)
- #17634 Fix comparison function signature (hebasto)
- #18062 Fix unintialized WalletView::progressDialog (promag)
- #17416 Appveyor improvement - text file for vcpkg package list (sipsorcery)
- #17488 fix "bitcoind already running" warnings on macOS (fanquake)
- #17980 add missing #include to fix compiler errors (kallewoof)
### Platform support
- #17736 Update msvc build for Visual Studio 2019 v16.4 (sipsorcery)
- #17364 Updates to appveyor config for VS2019 and Qt5.9.8 + msvc project fixes (sipsorcery)
- #17887 bug-fix macos: give free bytes to `F_PREALLOCATE` (kallewoof)
### Miscellaneous
- #17897 init: Stop indexes on shutdown after ChainStateFlushed callback (jimpo)
- #17450 util: Add missing headers to util/fees.cpp (hebasto)
- #17654 Unbreak build with Boost 1.72.0 (jbeich)
- #17857 scripts: Fix symbol-check & security-check argument passing (fanquake)
- #17762 Log to net category for exceptions in ProcessMessages (laanwj)
- #18100 Update univalue subtree (MarcoFalke)

0.17.1 change log
=================

### P2P protocol and network code
- #14685 `9406502` Fix a deserialization overflow edge case (kazcw)
- #14728 `b901578` Fix uninitialized read when stringifying an addrLocal (kazcw)

### Wallet
- #14441 `5150acc` Restore ability to list incoming transactions by label (jnewbery)
- #13546 `91fa15a` Fix use of uninitialized value `bnb_used` in CWallet::CreateTransaction(…) (practicalswift)
- #14310 `bb90695` Ensure wallet is unlocked before signing (gustavonalle)
- #14690 `5782fdc` Throw error if CPubKey is invalid during PSBT keypath serialization (instagibbs)
- #14852 `2528443` backport: [tests] Add `wallet_balance.py` (MarcoFalke)
- #14196 `3362a95` psbt: always drop the unnecessary utxo and convert non-witness utxo to witness when necessary (achow101)
- #14588 `70ee1f8` Refactor PSBT signing logic to enforce invariant and fix signing bug (gwillen)
- #14424 `89a9a9d` Stop requiring imported pubkey to sign non-PKH schemes (sipa, MeshCollider)

### RPC and other APIs
- #14417 `fb9ad04` Fix listreceivedbyaddress not taking address as a string (etscrivner)
- #14596 `de5e48a` Bugfix: RPC: Add `address_type` named param for createmultisig (luke-jr)
- #14618 `9666dba` Make HTTP RPC debug logging more informative (practicalswift)
- #14197 `7bee414` [psbt] Convert non-witness UTXOs to witness if witness sig created (achow101)
- #14377 `a3fe125` Check that a separator is found for psbt inputs, outputs, and global map (achow101)
- #14356 `7a590d8` Fix converttopsbt permitsigdata arg, add basic test (instagibbs)
- #14453 `75b5d8c` Fix wallet unload during walletpassphrase timeout (promag)

### GUI
- #14403 `0242b5a` Revert "Force TLS1.0+ for SSL connections" (real-or-random)
- #14593 `df5131b` Explicitly disable "Dark Mode" appearance on macOS (fanquake)

### Build system
- #14647 `7edebed` Remove illegal spacing in darwin.mk (ch4ot1c)
- #14698 `ec71f06` Add bitcoin-tx.exe into Windows installer (ken2812221)

### Tests and QA
- #13965 `29899ec` Fix extended functional tests fail (ken2812221)
- #14011 `9461f98` Disable wallet and address book Qt tests on macOS minimal platform (ryanofsky)
- #14180 `86fadee` Run all tests even if wallet is not compiled (MarcoFalke)
- #14122 `8bc1bad` Test `rpc_help.py` failed: Check whether ZMQ is enabled or not (Kvaciral)
- #14101 `96dc936` Use named args in validation acceptance tests (MarcoFalke)
- #14020 `24d796a` Add tests for RPC help (promag)
- #14052 `7ff32a6` Add some actual witness in `rpc_rawtransaction` (MarcoFalke)
- #14215 `b72fbab` Use correct python index slices in example test (sdaftuar)
- #14024 `06544fa` Add `TestNode::assert_debug_log` (MarcoFalke)
- #14658 `60f7a97` Add test to ensure node can generate all rpc help texts at runtime (MarcoFalke)
- #14632 `96f15e8` Fix a comment (fridokus)
- #14700 `f9db08e` Avoid race in `p2p_invalid_block` by waiting for the block request (MarcoFalke)
- #14845 `67225e2` Add `wallet_balance.py` (jnewbery)

### Documentation
- #14161 `5f51fd6` doc/descriptors.md tweaks (ryanofsky)
- #14276 `85aacc4` Add autogen.sh in ARM Cross-compilation (walterwhite81)

Credits
=======

Thanks to everyone who directly contributed to this release:

- Adam Jonas
- Fabian Jahr
- Harris
- Jan Beich
- Michael Chrostowski
As well as to everyone that helped with translations on
- Eric Scrivner
- fanquake
- fridokus
- Glenn Willen
- Gregory Sanders
- gustavonalle
- John Newbery
- Jon Layton
- Jonas Schnelli
- João Barbosa
- Kaz Wesley
- Kvaciral
- Luke Dashjr
- MarcoFalke
- MeshCollider
- Pieter Wuille
- practicalswift
- Russell Yanofsky
- Sjors Provoost
- Suhas Daftuar
- Tim Ruffing
- Walter
- Wladimir J. van der Laan

As well as everyone that helped translating on [Transifex](https://www.transifex.com/projects/p/bitcoin/).
