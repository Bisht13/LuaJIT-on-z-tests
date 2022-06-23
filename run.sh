#!/bin/bash
# set -x

#-----1st Test-----

# run test
echo "Running basic tests"
gcc -DDASM_CHECKS -std=gnu99 -Wall -Werror -g -x c -lm -o minilua ../../src/host/minilua.c
./minilua ../dynasm.lua test_z_inst.c | gcc -DDASM_CHECKS -std=gnu99 -Wall -Werror -g -x c -o test_z_inst -
./test_z_inst
ec=$?
echo ""

#-----2nd Test-----

# run test
echo "Running non-jitted version"
gcc -DDASM_CHECKS -std=gnu99 -Wall -Werror -g -x c -o bf_c bf_c.c
./bf_c mandelbrot.bf
echo ""
echo "Running jitted version"
./minilua ../dynasm.lua bf_dynasm_s390x.c | gcc -DDASM_CHECKS -std=gnu99 -Wall -Werror -g -x c -o bf_dynasm_s390x -
./bf_dynasm_s390x mandelbrot.bf
echo ""

# cleanup
rm -f ./test_z_inst ./minilua ./bf_c ./bf_dynasm_s390x

# exit
exit $ec
