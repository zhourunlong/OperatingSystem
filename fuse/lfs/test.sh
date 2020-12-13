rm lfs.data
mkdir test_zhy
./fuse test_zhy
cp tests/testfile2.cpp test_zhy/
cd test_zhy
g++ testfile2.cpp -o tst
./tst
cd ..
fusermount -u test_zhy
rm -rf test_zhy
