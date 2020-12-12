cd ../lfs

if [[ ! -d disk100Mi ]]; then
    mkdir -p disk100Mi
fi

scons -c
scons

./fuse disk100Mi
