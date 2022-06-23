#!/bin/bash
# set -x

# run test
gcc -DDASM_CHECKS -std=gnu99 -Wall -Werror -g -x c -lm -o minilua ../../src/host/minilua.c
./minilua ../dynasm.lua test_z_inst.c | gcc -DDASM_CHECKS -std=gnu99 -Wall -Werror -g -x c -o test_z_inst -
./test_z_inst
ec=$?

# cleanup
rm -f ./test_z_inst
rm -f ./minilua

# exit
exit $ec
