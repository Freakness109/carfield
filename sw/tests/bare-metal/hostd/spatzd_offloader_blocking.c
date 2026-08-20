// Copyright 2025 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Alessandro Ottaviano <aottaviano@iis.ee.ethz.ch>
// Mattia Sinigaglia <mattia.sinigaglia5@unibo.it>

#include <stdio.h>
#include <stdlib.h>

#include "io.h"
#include "regs/cheshire.h"
#include "dif/clint.h"
#include "dif/uart.h"
#include "params.h"
#include "util.h"
#include "car_util.h"
#include "car_params.h"
#include "printf.h"
#include "car_iommu.h"

#define CAR_FP_CLUSTER_MBOX_CORE0_ID 0
#define CAR_FP_CLUSTER_MBOX_CORE1_ID 1

#define SPATZ_CLUSTER_PERIPHERAL_EOC_REG_ADDR 0x51020060

int main(void) {

    // Init the HW

    iommu_init();
    // mapping needs to be a multiple of 2MiB, so map both interleaved
    // and non-interleaved port 1 of L2 SPM
    iommu_map(0x78000000, 0x78000000, 0x200000);
    // map 1 GiB of RAM
    iommu_map(0x80000000, 0x80000000, 0x40000000);
    // map most peripherals
    iommu_map(0x03000000, 0x03000000, 0x200000);
    iommu_1lvl();

    // Spatz Island
    car_enable_domain(CAR_SPATZ_RST);

    load_binary();
    fence();

    writew(ELF_BOOT_ADDR, CAR_FP_CLUSTER_PERIPHERAL_CLUSTER_BOOT_CONTROL_REG_ADDR(car_spatz_cluster));

    writew(1, CAR_MBOX_BASE_ADDR +  MBOX_INT_SND_SET_OFFSET + (CAR_FP_CLUSTER_MBOX_CORE0_ID*0x100));
    writew(1, CAR_MBOX_BASE_ADDR +  MBOX_INT_SND_SET_OFFSET + (CAR_FP_CLUSTER_MBOX_CORE1_ID*0x100));

    writew(1, CAR_MBOX_BASE_ADDR +  MBOX_INT_SND_EN_OFFSET + (CAR_FP_CLUSTER_MBOX_CORE0_ID*0x100));
    writew(1, CAR_MBOX_BASE_ADDR +  MBOX_INT_SND_EN_OFFSET + (CAR_FP_CLUSTER_MBOX_CORE1_ID*0x100));

    volatile uint32_t spatzd_corestatus;
    volatile uint32_t *spatzd_corestatus_addr = (volatile uint32_t*)CAR_FP_CLUSTER_PERIPHERAL_CORESTATUS_REG_ADDR(car_spatz_cluster);

    while (!readw(spatzd_corestatus_addr))
    	;
    spatzd_corestatus = readw(spatzd_corestatus_addr);

    //while (!readw((uint32_t *)SPATZ_CLUSTER_PERIPHERAL_EOC_REG_ADDR))
    //	;

    return spatzd_corestatus>>1;
}
