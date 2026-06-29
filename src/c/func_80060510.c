#include "globals.h"

/* Call the GPU dispatch-table method at +0x38 and return its result's sign bit.
 */
int func_80060510(void) {
  return (unsigned int)(*(int (**)(void))((char *)g_pGpuDispatchTable +
                                          0x38))() >>
         31;
}
