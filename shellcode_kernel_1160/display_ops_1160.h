#ifndef DISPLAY_OPS_1160_H
#define DISPLAY_OPS_1160_H

#include <stdint.h>

typedef struct {
  uint64_t ktext_offset;
  uint32_t arg1_edi;
  uint32_t mode;
  uint64_t arg2_rsi;
} ps5_display_op_1160;

extern const ps5_display_op_1160 ps5_display_ops_1100[29];
extern const ps5_display_op_1160 ps5_display_ops_1120[29];
extern const ps5_display_op_1160 ps5_display_ops_1140[29];
extern const ps5_display_op_1160 ps5_display_ops_1060[29];
extern const ps5_display_op_1160 ps5_display_ops_1160[29];

void run_ps5_display_ops_1160(void);

#endif
