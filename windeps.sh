#!/bin/sh

mkdir -p windows
cp bin/modp.exe windows/
ldd windows/modp.exe | grep mingw64 | awk '{ print $3 }' | sort | uniq | xargs cp -t windows
strip windows/*
