#include "destroy.h"
#include "prefix.h"

void o_destroy(void* private_data) {
    logger(DEBUG, "DESTROY, %p\n", private_data);
}
