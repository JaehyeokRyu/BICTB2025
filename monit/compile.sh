#! /bin/bash

ext=${1##*.}
fname=`basename ${1} .${ext}`

echo "Compiling $fname.cc to $fname"
g++ \
  -I$INSTALL_DIR_PATH/include \
  -I$YAMLPATH/../include \
  -L$INSTALL_DIR_PATH/lib \
  -L$YAMLPATH \
  -Wl,-rpath,$INSTALL_DIR_PATH/lib \
  -Wl,-rpath,$YAMLPATH \
  ${fname}.cc \
  $INSTALL_DIR_PATH/lib/libdrcTB.so \
  -lyaml-cpp \
  `root-config --cflags --libs` \
  -o ${fname}
echo "Done!"
