#include "mentalsdklib.h"

int main(void)
{
    MentalInstanceInfo info = {
        .pApplicationName = "mental",
        .eInterfaceType   = MENTAL_INTERFACE_TUI,
    };
    MentalInstance inst = { .pInfo = &info, .pTui = NULL, .pP2P = NULL };

    if (mental_instance_create(&inst) != MENTAL_RESULT_TYPE_OK)
        return 1;

    mental_instance_destroy(&inst);
    return 0;
}
