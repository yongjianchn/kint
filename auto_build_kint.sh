#!/bin/bash

#get path of the script
TOP="$(pwd)"
this_script_dir="$(dirname "$0")"
cd $this_script_dir
this_script_sdir="$(pwd)"

echo "====================================="
echo "Build LLVM and Clang 3.1"
echo "====================================="

#download llvm and clang sources
echo "Download llvm and clang sources now..."
[ -d llvm-3.1 ] || mkdir llvm-3.1
cd llvm-3.1
svn co http://llvm.org/svn/llvm-project/llvm/tags/RELEASE_31/final llvm
if [ $? != 0 ]; then
	echo "Download llvm failed, stop."
	exit 1
fi
cd llvm/tools/
svn co http://llvm.org/svn/llvm-project/cfe/tags/RELEASE_31/final clang
if [ $? != 0 ]; then
	echo "Download clang failed, stop."
	exit 1
fi
cd ../projects/
svn co http://llvm.org/svn/llvm-project/compiler-rt/tags/RELEASE_31/final compiler-rt
if [ $? != 0 ]; then
	echo "Download compiler-rt failed, stop."
	exit 1
fi
cd ../..
echo "Download sources successfully."

#build and install llvm
echo "Configuring llvm start..."
[ -d build ] || mkdir build
cd build
[ -f Makefile ] || ../llvm/configure --enable-optimized --enable-targets=host --enable-bindings=none --enable-shared --enable-debug-symbols --prefix=$(pwd)/../install
if [ $? != 0 ]; then
	echo "Configuring llvm failed, stop."
	exit 1
fi
echo "Building llvm start..."
make -j `cat /proc/cpuinfo | grep processor |wc -l`
#this may got some errors, but not important
make install
if [ $? != 0 ]; then
	echo "Building llvm failed, stop."
	exit 1
fi

export PATH=${TOP}/llvm-3.1/install/bin:${PATH}
export C_INCLUDE_PATH=${TOP}/llvm-3.1/install/include:${C_INCLUDE_PATH}
export LD_LIBRARY_PATH=${TOP}/llvm-3.1/install/lib:${LD_LIBRARY_PATH}

#判断是否配置好llvm&clang 3.1的环境
clang_path=$(which clang)
if [ "$clang_path" != "" ]; then
    clang_version=$(clang --version | grep 3.1)
    if [ "$clang_version" != "" ]; then
        echo "Building and installing LLVM and Clang successfully."
    else
        echo "Building and installing LLVM and Clang failed, stop."
        exit 1
    fi
else
    echo "Building and installing LLVM and Clang fialed, stop."
    exit 1
fi

#download and build kint now
echo "Downloading kint sources..."
cd ${TOP}
[ -d kint ] || git clone git://g.csail.mit.edu/kint
if [ $? != 0 ]; then
    echo "Download failed, test stop. "
    exit 1
fi
echo "Download kint sources successfully."

cd ${TOP}/kint/
echo "Building kint now..."
autoreconf -fvi
mkdir build
cd build/
../configure
make -j `cat /proc/cpuinfo | grep processor |wc -l` 
if [ $? != 0 ]; then
    echo "Build kint failed, test stop. "
	echo "Try to erase '-Werror' flags in $(pwd)/src/Makefile and make again"
    exit 1
fi
echo "Build kint successfully."
