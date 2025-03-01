#!/usr/bin/env fish

set bin_folder (realpath (status dirname))
set krnl_src (dirname $bin_folder)

if test -n "$PO"
    set build_po "$PO:$bin_folder"
else
    set build_po $bin_folder
end

if test (count $argv) -eq 0
    set kmake_args -C $krnl_src distclean wsl2_defconfig bzImage
else
    set kmake_args $argv
end

PO="$build_po" make HOSTLDFLAGS=-fuse-ld=lld LLVM=1 $kmake_args -j16
