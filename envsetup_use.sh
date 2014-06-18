cd $(dirname $(readlink -f ${BASH_SOURCE[0]}) )
TOP=`pwd`
echo $TOP
cd - 
export PATH=$TOP/llvm-3.1/install/bin:${PATH}
export PATH=$TOP/kint/build/bin:$PATH
