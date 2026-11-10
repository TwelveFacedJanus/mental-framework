#include "mentalsdklib.h"



int main(void) {
    MentalInstanceInfo info = {
        .pApplicationName = "mental",
        .eInterfaceType   = MENTAL_INTERFACE_TUI,
    };
    MentalInstance inst = { .pInfo = &info, .pTui = NULL };

    mental_instance_create(&inst);
    if (inst.pTui) mentalTuiDestroy(inst.pTui);
    return 0;
}
