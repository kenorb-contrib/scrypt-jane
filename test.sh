#!/bin/sh

if [ "$2" == "" ]; then
	echo "Usage: ./test.sh cipher hash [arguments]";
	exit
fi

emcc scrypt-jane.c -O3 -DSCRYPT_$1 -DSCRYPT_$2 $3 -s EXPORTED_FUNCTIONS='["_scrypt_hex"]' -s EXTRA_EXPORTED_RUNTIME_METHODS='["ccall", "cwrap"]' -s ASSERTIONS=1 -s TOTAL_MEMORY=20971520 -o test.js
RC=$?
if [[ $RC -ne 0 ]]; then
	echo "$1/$2: failed to compile "
	return
fi
firefox test.html
