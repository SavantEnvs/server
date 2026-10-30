/*
  Ported from OSS-Fuzz projects/mariadb/fuzz_json.c (Apache-2.0, Google LLC).
  Adapted to the current json_locate_key() signature, which now takes a
  json_engine_t whose nesting stack the caller must initialise.
*/
#include "json_common.h"

#define FUZZ_KEY_SIZE 32

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
  if (size < FUZZ_KEY_SIZE)
    return -1;

  char fuzz_key[FUZZ_KEY_SIZE + 1];
  memcpy(fuzz_key, data, FUZZ_KEY_SIZE);
  fuzz_key[FUZZ_KEY_SIZE]= '\0';

  const char *js= (const char *) data + FUZZ_KEY_SIZE;
  size_t js_len= size - FUZZ_KEY_SIZE;

  const char *value_start;
  int value_len;
  json_get_object_key(js, js + js_len, fuzz_key, &value_start, &value_len);
  json_get_array_item(js, js + js_len, (int) (data[0] & 15), &value_start,
                      &value_len);

  MEM_ROOT root;
  json_engine_t je;
  json_fuzz_begin(&root, &je);
  const char *key_start, *key_end;
  int comma_pos;
  json_locate_key(&je, js, js + js_len, fuzz_key, &key_start, &key_end,
                  &comma_pos);
  free_root(&root, MYF(0));
  return 0;
}
