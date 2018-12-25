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
If your node has a txindex, the txindex db will be migrated the first time you
run 0.17.0 or newer, which may take up to a few hours. Your node will not be
functional until this migration completes.

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

0.17.x change log
=================

(todo)

Credits
=======

Thanks to everyone who directly contributed to this release:

- Adam Jonas
- Fabian Jahr
- Harris
- Jan Beich
- Michael Chrostowski
As well as to everyone that helped with translations on

As well as everyone that helped translating on [Transifex](https://www.transifex.com/projects/p/bitcoin/).
