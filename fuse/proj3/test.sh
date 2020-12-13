#!/bin/bash
cd ../lfs
cp tests disk100Mi/tests -r
cd disk100Mi/tests

g++ testfile.cpp -o testfile
mkdir Testfile
cp testfile Testfile/run
cd Testfile
./run
cd ..

g++ testfile2.cpp -o testfile2
mkdir Testfile2
cp testfile2 Testfile2/run
cd Testfile2
./run
cd ..

g++ testmkdir.cpp -o testmkdir
mkdir Testmkdir
cp testmkdir Testmkdir/run
cd Testmkdir
./run 999
cd ..

g++ testrmdir.cpp -o testrmdir
mkdir Testrmdir
cp testrmdir Testrmdir/run
cd Testrmdir
./run 999
cd ..

g++ testconcurrency.cpp -o testconcurrency -std=c++11 -lpthread
mkdir Testconcurrency
cp testconcurrency Testconcurrency/run
cd Testconcurrency
./run 12 50
cd ..

cd ../..
fusermount -u disk100Mi

