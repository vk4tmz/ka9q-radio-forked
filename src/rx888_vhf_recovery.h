#ifndef RX888_VHF_RECOVERY_H
#define RX888_VHF_RECOVERY_H
#include <stdbool.h>
#include <stdint.h>
/* A failed startup is not an HF success. Attempt both independent cleanup
 * actions even if the first fails; never publish VHF readiness on failure.
 */
typedef int (*rx888_recovery_action)(void *,uint32_t);
typedef struct {
  bool ready;
  bool cleanup_ok;
  double frequency;
} rx888_vhf_recovery_state;
static inline int rx888_vhf_recover(rx888_vhf_recovery_state *state,
                                     void *context,
                                     rx888_recovery_action disable_clk2,
                                     rx888_recovery_action select_hf,
                                     uint32_t hf_gpios){
  if(!state || !disable_clk2 || !select_hf)return -1;
  state->ready=false;
  state->frequency=0;
  state->cleanup_ok=false;
  int const a=disable_clk2(context,0);
  int const b=select_hf(context,hf_gpios);
  if(a!=0 || b!=0)return -1;
  state->cleanup_ok=true;
  return 0;
}
#endif
