# Copyright 2022 ETH Zurich and University of Bologna.
# Solderpad Hardware License, Version 0.51, see LICENSE for details.
# SPDX-License-Identifier: SHL-0.51
#

# set up environment variables for rtl simulation
ROOTD=$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")/.." && pwd)
echo $ROOTD
export PATH=/usr/pack/riscv-1.0-kgf/riscv64-gcc-14.2.0/bin:$PATH # RV64 GCC toolchain
export SAFED_RISCV=/usr/pack/riscv-1.0-kgf/pulp-gcc-2.6.0/bin/riscv32-unknown-elf
export PULPD_RISCV=/usr/pack/pulpsdk-1.0-kgf/artifactory/pulp-sdk-release/pkg/pulp_riscv_gcc/1.0.16/bin/riscv32-unknown-elf
export QUESTA=questa-2023.4
if [[ -d "$ROOTD/spatz/install/llvm" ]]; then
	export LLVM_INSTALL_DIR="$ROOTD/spatz/install/llvm"
else
	export LLVM_INSTALL_DIR=/usr/pack/riscv-1.0-kgf/spatz-llvm-2023.08.10
fi
if [[ -d "$ROOTD/spatz/install/llvm" ]]; then
	export GCC_INSTALL_DIR="$ROOTD/spatz/install/riscv-gcc/"
else
	export GCC_INSTALL_DIR=/usr/pack/riscv-1.0-kgf/spatz-gcc-7.1.1
fi

export TOOLS_DIR=/usr/scratch2/pisoc10/spatz/tools

export LLVM_INSTALL_DIR=$TOOLS_DIR/riscv-llvm
export GCC_INSTALL_DIR=/usr/pack/riscv-1.0-kgf/spatz-gcc-7.1.1

export VERILATOR_INSTALL_DIR=$TOOLS_DIR/verilator
export VERILATOR_ROOT=$VERILATOR_INSTALL_DIR/share/verilator
export LD_PRELOAD=$VERILATOR_INSTALL_DIR/libstdc++.so.6

export CC=gcc-11.2.0
export CXX=g++-11.2.0

export CMAKE=/usr/bin/cmake

export BENDER_INSTALL_DIR=/usr/local/bin
