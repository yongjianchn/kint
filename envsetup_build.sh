cd $(dirname $(readlink -f ${BASH_SOURCE[0]}) )
TOP=`pwd`
echo $TOP
cd - 
export PATH=${TOP}/llvm-3.1/install/bin:${PATH}
export C_INCLUDE_PATH=${TOP}/llvm-3.1/install/include:${C_INCLUDE_PATH}
export LD_LIBRARY_PATH=${TOP}/llvm-3.1/install/lib:${LD_LIBRARY_PATH}
