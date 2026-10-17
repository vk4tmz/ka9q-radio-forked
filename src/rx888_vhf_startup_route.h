#ifndef RX888_VHF_STARTUP_ROUTE_H
#define RX888_VHF_STARTUP_ROUTE_H
#include <stdbool.h>
/* Route selection depends on whether VHF was actually attempted, not
 * solely on the frontend frequency sentinel.
 */
typedef enum { RX888_STARTUP_HF, RX888_STARTUP_VHF_FAILED, RX888_STARTUP_VHF_READY } rx888_startup_route;
static inline rx888_startup_route rx888_choose_startup_route(bool attempted,bool ready){
  if(!attempted) return RX888_STARTUP_HF;
  return ready ? RX888_STARTUP_VHF_READY : RX888_STARTUP_VHF_FAILED;
}
/* Any attempted but unsuccessful VHF startup rejects setup, even if cleanup worked. */
static inline int rx888_startup_recovery_result(rx888_startup_route route,int recovery_rc){
  (void)recovery_rc;
  return route == RX888_STARTUP_VHF_FAILED ? -1 : 0;
}
#endif
