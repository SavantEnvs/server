/*
  Input: [charset selector][path length][path text][JSON document].
  Parses the path, then searches the document with json_find_path().
*/
#include "json_common.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
  if (size < 2)
    return -1;
  CHARSET_INFO *cs= json_fuzz_charset(data[0]);
  size_t plen= data[1];
  data+= 2;
  size-= 2;
  if (plen > size)
    plen= size;
  const uchar *path= data;
  const uchar *js= data + plen;
  size_t js_len= size - plen;

  MEM_ROOT root;
  json_engine_t je;
  json_path_t p;
  MEM_ROOT_DYNAMIC_ARRAY counters;
  json_fuzz_begin(&root, &je);
  mem_root_dynamic_array_init(&root, PSI_INSTRUMENT_MEM, &counters, sizeof(int),
                              NULL, JSON_DEPTH_DEFAULT, JSON_DEPTH_INC,
                              MYF(0));
  mem_root_dynamic_array_init(&root, PSI_INSTRUMENT_MEM, &p.steps,
                              sizeof(json_path_step_t), NULL,
                              JSON_DEPTH_DEFAULT, JSON_DEPTH_INC, MYF(0));

  if (json_path_setup(&p, cs, path, path + plen) == 0 &&
      json_scan_start(&je, cs, js, js + js_len) == 0)
  {
    json_path_step_t *cur= (json_path_step_t *) p.steps.buffer;
    while (json_find_path(&je, &p, &cur, &counters) == 0)
    {
      if (json_read_value(&je))
        break;
      if (json_value_scalar(&je))
      {
        if (json_scan_next(&je))
          break;
      }
      else if (json_skip_level(&je) || json_scan_next(&je))
        break;
    }
  }
  free_root(&root, MYF(0));
  return 0;
}
