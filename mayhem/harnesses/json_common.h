#ifndef MAYHEM_JSON_COMMON_H
#define MAYHEM_JSON_COMMON_H
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <my_global.h>
#include <my_sys.h>
#include <json_lib.h>
#include "fuzz_init.h"

/* One MEM_ROOT per fuzz input; the JSON engine's nesting stack lives in it. */
static inline void json_fuzz_begin(MEM_ROOT *root, json_engine_t *je)
{
  je->killed_ptr= NULL;
  init_alloc_root(PSI_INSTRUMENT_MEM, root, BLOCK_SIZE_JSON_DYN_ARRAY, 0,
                  MYF(0));
  mem_root_dynamic_array_init(root, PSI_INSTRUMENT_MEM, &je->stack, sizeof(int),
                              NULL, JSON_DEPTH_DEFAULT, JSON_DEPTH_INC,
                              MYF(0));
}

static inline CHARSET_INFO *json_fuzz_charset(uint8_t selector)
{
  switch (selector & 3)
  {
  case 0:  return &my_charset_utf8mb3_general_ci;
  case 1:  return &my_charset_utf8mb4_general_ci;
  case 2:  return &my_charset_latin1;
  default: return &my_charset_ucs2_general_ci;
  }
}
#endif
