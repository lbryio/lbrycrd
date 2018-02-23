src/lbrycrd-cli stop
sleep 5

rm -rf ~/.lbrycrd/regtest
rm -rf /usr/src/temp/regtest
cp lbrycrd.conf ~/.lbrycrd/lbrycrd.conf
cp lbrycrd.conf /usr/src/temp/lbrycrd.conf

# use valgrind
#valgrind --tool=massif --massif-out-file=lbrycrd_massif.out src/lbrycrdd -regtest
#/usr/src/KDevelop.AppImage
#gdb -ex run --args src/lbrycrdd -regtest

