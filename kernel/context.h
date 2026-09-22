#ifndef CONTEXT_H
#define CONTEXT_H

#include <stdint.h>

void context_switch(
    uint32_t* old_stack_pointer,
    uint32_t new_stack_pointer
);

void context_start(
    uint32_t* old_stack_pointer,
    uint32_t new_stack_pointer
);

void context_return_to_stack(
    uint32_t stack_pointer
);

#endif
