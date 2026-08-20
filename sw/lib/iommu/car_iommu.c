#include "car_iommu.h"
#include <stdint.h>
#include <stdbool.h>

#define NOINIT __attribute__((section(".noinit.data")))
#define PAGEALIGN __attribute__((aligned(PAGE_SIZE)))

/* IOMMU hw structure only visible inside IOMMU driver */
static const iommu_t *iommu = (const iommu_t *)0x2000a000;

uint64_t command_queue[CQ_N_ENTRIES * 2] NOINIT PAGEALIGN;
uint64_t fault_queue[FQ_N_ENTRIES * 4] NOINIT PAGEALIGN;

ddt_t root_ddt[DDT_N_ENTRIES] NOINIT PAGEALIGN;

pte_t iommu_gigapage_entries[1][PAGE_SIZE / sizeof(pte_t)] NOINIT PAGEALIGN;
pte_t iommu_megapage_entries[512][PAGE_SIZE / sizeof(pte_t)] NOINIT PAGEALIGN;

// this takes a gig, and we don't have an allocator to have a sparse table
// so just leave it out and have 2MiB megapages instead of 4KiB pages
// pte_t iommu_page_entries[512][512][PAGE_SIZE/sizeof(pte_t)];

static inline uint64_t read64(uintptr_t addr) {
  return *((volatile uint64_t *)addr);
}

static inline uint32_t read32(uintptr_t addr) {
  return *((volatile uint32_t *)addr);
}

static inline uint16_t read16(uintptr_t addr) {
  return *((volatile uint16_t *)addr);
}

static inline uint8_t read8(uintptr_t addr) {
  return *((volatile uint8_t *)addr);
}

static inline void write64(uintptr_t addr, uint64_t val) {
  *((volatile uint64_t *)addr) = val;
}

static inline void write32(uintptr_t addr, uint32_t val) {
  *((volatile uint32_t *)addr) = val;
}

static inline void write16(uintptr_t addr, uint16_t val) {
  *((volatile uint16_t *)addr) = val;
}

static inline void write8(uintptr_t addr, uint8_t val) {
  *((volatile uint8_t *)addr) = val;
}

static inline void cmo_clean(uintptr_t addr, size_t size) {
  uint16_t cache_line_size = 16;
  if (addr & 15) {
    addr -= addr & 15;
    size += 16;
  }
  for (uintptr_t end = addr + size; addr < end; addr += cache_line_size)
    asm volatile("CBO.CLEAN (%0)"
                 : /* no output */
                 : "r"(addr)
                 : /* no clobber */);
}
static inline void cmo_flush(uintptr_t addr, size_t size) {
  uint16_t cache_line_size = 16;
  if (addr & 15) {
    addr -= addr & 15;
    size += 16;
  }
  for (uintptr_t end = addr + size; addr < end; addr += cache_line_size)
    asm volatile("CBO.FLUSH (%0)"
                 : /* no output */
                 : "r"(addr)
                 : /* no clobber */);
}

void iommu_off() {
  // Program ddtp register with DDT mode and root DDT base PPN
  uintptr_t ddtp =
      ((((uintptr_t)root_ddt) >> 2) & DDTP_PPN_MASK) | (DDTP_MODE_OFF);

  write64((uintptr_t)&iommu->ddtp, ddtp);
}

void iommu_bare() {
  // Program ddtp register with DDT mode and root DDT base PPN
  uintptr_t ddtp =
      ((((uintptr_t)root_ddt) >> 2) & DDTP_PPN_MASK) | (DDTP_MODE_BARE);

  write64((uintptr_t)&iommu->ddtp, ddtp);
}

void iommu_1lvl() {
  // Program ddtp register with DDT mode and root DDT base PPN
  uintptr_t ddtp =
      ((((uintptr_t)root_ddt) >> 2) & DDTP_PPN_MASK) | (DDTP_MODE_1LVL);

  root_ddt->fsc = IOSATP_MODE_SV39 |
                 (((uintptr_t)&iommu_gigapage_entries[0][0]) >> PAGE_SHIFT);
  root_ddt->ta = 0;
  root_ddt->iohgatp = IOHGATP_MODE_BARE;
  root_ddt->tc = DC_TC_VALID;
  cmo_flush((uintptr_t)&root_ddt, sizeof(root_ddt));

  write64((uintptr_t)&iommu->ddtp, ddtp);
}

static void iommu_install_gigapage(uintptr_t virt, uintptr_t phys) {
  iommu_gigapage_entries[0][PTE_INDEX(0, virt)] =
      ((phys >> 2) & PTE_ADDR_MSK) | (PTE_AD | PTE_V | PTE_U | PTE_RWX);
  cmo_clean((uintptr_t)&iommu_gigapage_entries[0][PTE_INDEX(0, virt)],
            sizeof(pte_t));
}

static void iommu_install_megapage(uintptr_t virt, uintptr_t phys) {
  iommu_gigapage_entries[0][PTE_INDEX(0, virt)] =
      (((uintptr_t)&iommu_megapage_entries[PTE_INDEX(0, virt)]) >> 2) | PTE_V;
  cmo_clean((uintptr_t)&iommu_gigapage_entries[0][PTE_INDEX(0, virt)],
            sizeof(pte_t));

  iommu_megapage_entries[PTE_INDEX(0, virt)][PTE_INDEX(1, virt)] =
      ((phys >> 2) & PTE_ADDR_MSK) | (PTE_AD | PTE_V | PTE_U | PTE_RWX);
  cmo_clean((uintptr_t)&iommu_megapage_entries[PTE_INDEX(0, virt)]
                                              [PTE_INDEX(1, virt)],
            sizeof(pte_t));
}

static void iommu_cq_init(void) {
  // Configure cqb with base PPN of the queue and size as log2(N)
  write64((uintptr_t)&iommu->cqb,
          ((((uintptr_t)command_queue) >> 2) & CQB_PPN_MASK) | CQ_LOG2SZ_1);

  // Set cqt equal to cqh
  write32((uintptr_t)&iommu->cqt, read32((uintptr_t)&iommu->cqh));

  // Write 1 to cqcsr.cqen to enable the CQ
  write32((uintptr_t)&iommu->cqcsr, CQCSR_CQEN);

  // Poll cqcsr.cqon until it reads 1
  while (!(read32((uintptr_t)&iommu->cqcsr) & (CQCSR_CQON | CQCSR_BUSY)))
    ;
}

static void iommu_fq_init(void) {
  // Configure fqb with base PPN of the queue and size as log2(N)
  write64((uintptr_t)&iommu->fqb,
          ((((uintptr_t)fault_queue) >> 2) & FQB_PPN_MASK) | FQ_LOG2SZ_1);

  // Set fqh equal to fqt
  write32((uintptr_t)&iommu->fqh, read32((uintptr_t)&iommu->fqt));

  // Write 1 to fqcsr.fqen to enable the FQ
  write32((uintptr_t)&iommu->fqcsr, FQCSR_FQEN);

  // Poll fqcsr.fqon until it reads 1
  while (!(read32((uintptr_t)&iommu->fqcsr) & (FQCSR_FQON | FQCSR_BUSY)))
    ;
}

void iommu_init(void) {
  iommu_cq_init();
  iommu_fq_init();
}

static void iommu_iotinval_vma(void) {
  uint32_t tail_index = read32((uintptr_t)&iommu->cqt);
  uint32_t head_index = read32((uintptr_t)&iommu->cqh);

  while (((tail_index + 1) % CQ_N_ENTRIES) == head_index) {
    // queue is full
    head_index = read32((uintptr_t)&iommu->cqh);
  }

  command_queue[tail_index * 2 + 0] = IOTINVAL | VMA;
  command_queue[tail_index * 2 + 1] = 0;

  cmo_flush((uintptr_t)&command_queue[tail_index * 2], 2 * sizeof(uint64_t));
  write32((uintptr_t)&iommu->cqt, (tail_index + 1) % CQ_N_ENTRIES);
}

bool iommu_map(uintptr_t virt, uintptr_t phys, size_t size) {
  if (virt & ~SUPERPAGE_MASK(1))
    return false;
  if (phys & ~SUPERPAGE_MASK(1))
    return false;
  if (size & ~SUPERPAGE_MASK(1))
    return false;
  // 1. install megapages until we have gigapage alignment
  while (size && (virt & ~SUPERPAGE_MASK(0)) && (phys & SUPERPAGE_MASK(0))) {
    iommu_install_megapage(virt, phys);
    virt += SUPERPAGE_SIZE(1);
    phys += SUPERPAGE_SIZE(1);
    size -= SUPERPAGE_SIZE(1);
  }

  // 2. install gigapages until the size is too small
  while (size >= SUPERPAGE_SIZE(0)) {
    iommu_install_gigapage(virt, phys);
    virt += SUPERPAGE_SIZE(0);
    phys += SUPERPAGE_SIZE(0);
    size -= SUPERPAGE_SIZE(0);
  }

  // 3. install megapages for the rest
  while (size) {
    iommu_install_megapage(virt, phys);
    virt += SUPERPAGE_SIZE(1);
    phys += SUPERPAGE_SIZE(1);
    size -= SUPERPAGE_SIZE(1);
  }

  // 4. flush the IOMMU
  iommu_iotinval_vma();

  return true;
}
