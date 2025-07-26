#include "camera_register.h"

#include <string.h>

struct camera_register *camera_register_get(struct camera_registers_struct *structure, const char *name)
{
    uint32_t i;

    // Find the register
    for (i = 0; i < structure->length; ++i) {
        // Check which register matches a name
        if (strcmp(structure->registers[ i ].name, name) == 0) {
            return &structure->registers[ i ];
        }
    }

    // Register not found
    return NULL;
}
