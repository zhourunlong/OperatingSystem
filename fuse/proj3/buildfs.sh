#!/bin/bash
cd ../lfs
fusermount -u disk100Mi
if [[ ! -d disk100Mi ]]; then
    mkdir -p disk100Mi
fi

scons -c
scons
rm lfs.data
./fuse disk100Mi
