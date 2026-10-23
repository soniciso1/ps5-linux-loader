// This file is shared between main payload and kernel shellcode
#ifndef SHELLCODE_KERNEL_ARGS_H
#define SHELLCODE_KERNEL_ARGS_H

#include <stdint.h>

typedef struct {
  uint16_t fw_version;
  uint64_t ktext;
  uint64_t kdata;
  uint64_t dmap_base;
  uint64_t fun_printf;
  uint64_t fun_hv_iommu_set_buffers;
  uint64_t fun_hv_iommu_wait_completion;
  uint64_t fun_smp_rendezvous;
  uint64_t fun_smp_no_rendevous_barrier;
  uint64_t fun_transmitter_control;
  uint64_t fun_mp3_initialize;
  uint64_t fun_mp3_invoke;
  uint64_t g_vbios;
  uint64_t iommu_softc;
  uint64_t kernel_uart_override;
  uint64_t kernel_cfi_check;
  uint64_t hv_handle_vmexit_pa;
  uint64_t hv_code_cave_pa;
  uint64_t linux_info_va; // To relocate by kernel shellcode
  uint64_t data_cave_va;  // ktext + DATA_CAVE, for break_hv completion wait (FW_1160 only)
} shellcode_kernel_args;

extern shellcode_kernel_args args; // Declared on main.c

#endif
