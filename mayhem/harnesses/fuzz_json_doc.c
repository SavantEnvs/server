/* Walks a whole JSON document with the json_lib engine and its helpers. */
#include "json_common.h"

static void walk(json_engine_t *je, CHARSET_INFO *cs)
{
  uchar buf[256];
  do
  {
    switch (je->state)
    {
    case JST_KEY:
      while (json_read_keyname_chr(je) == 0) {}
      if (je->s.error)
        return;
      break;
    case JST_VALUE:
      if (json_read_value(je))
        return;
      if (je->value_type == JSON_VALUE_STRING)
      {
        int n= json_unescape(cs, je->value, je->value + je->value_len,
                             &my_charset_utf8mb4_bin, buf, buf + sizeof(buf));
        (void) n;
      }
      else if (!json_value_scalar(je))
      {
        if (json_skip_level(je))
          return;
      }
      break;
    default:
      break;
    }
  } while (json_scan_next(je) == 0);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
  if (size < 1)
    return -1;
  CHARSET_INFO *cs= json_fuzz_charset(data[0]);
  const uchar *js= data + 1;
  size_t len= size - 1;

  MEM_ROOT root;
  json_engine_t je;
  json_fuzz_begin(&root, &je);

  json_valid((const char *) js, len, cs, &je);

  if (json_scan_start(&je, cs, js, js + len) == 0)
    walk(&je, cs);

  json_path_t p;
  mem_root_dynamic_array_init(&root, PSI_INSTRUMENT_MEM, &p.steps,
                              sizeof(json_path_step_t), NULL,
                              JSON_DEPTH_DEFAULT, JSON_DEPTH_INC, MYF(0));
  if (json_get_path_start(&je, cs, js, js + len, &p) == 0)
    while (json_get_path_next(&je, &p) == 0) {}

  DYNAMIC_STRING result;
  MEM_ROOT norm_root;
  MEM_ROOT_DYNAMIC_ARRAY stack;
  json_engine_t norm_je;
  init_alloc_root(PSI_INSTRUMENT_MEM, &norm_root, BLOCK_SIZE_JSON_DYN_ARRAY, 0,
                  MYF(0));
  mem_root_dynamic_array_init(&norm_root, PSI_INSTRUMENT_MEM, &norm_je.stack,
                              sizeof(int), NULL, JSON_DEPTH_DEFAULT,
                              JSON_DEPTH_INC, MYF(0));
  norm_je.killed_ptr= NULL;
  mem_root_dynamic_array_init(&norm_root, PSI_INSTRUMENT_MEM, &stack,
                              sizeof(void *), NULL, JSON_DEPTH_DEFAULT,
                              JSON_DEPTH_INC, MYF(0));
  init_dynamic_string(&result, NULL, 0, 0);
  json_normalize(&result, (const char *) js, len, cs, &norm_root, &norm_je,
                 &stack);
  dynstr_free(&result);
  free_root(&norm_root, MYF(0));

  free_root(&root, MYF(0));
  return 0;
}
