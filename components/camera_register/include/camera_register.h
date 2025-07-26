#pragma once

#include <inttypes.h>

/* Enums */
enum camera_register_access { CAMREG_ACCESS_READ = 1 << 0, CAMREG_ACCESS_WRITE = 1 << 1 };

/* Structures */
struct camera_register {
    uint8_t address;
    const char *name;
    enum camera_register_access access;
};

struct camera_registers_struct {
    struct camera_register *registers;
    uint32_t length;
};

/* Functions */
struct camera_register *camera_register_get(struct camera_registers_struct *structure, const char *name);
