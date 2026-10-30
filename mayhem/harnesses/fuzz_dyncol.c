/* mysys/ma_dyncol.c: parsing/serialising the dynamic-column BLOB format. */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <my_global.h>
#include <my_sys.h>
#include <ma_dyncol.h>
#include "fuzz_init.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
  if (size < 1)
    return -1;
  DYNAMIC_COLUMN col;
  mariadb_dyncol_init(&col);
  col.str= (char *) malloc(size);
  memcpy(col.str, data, size);
  col.length= size;
  col.max_length= size;

  if (mariadb_dyncol_check(&col) == ER_DYNCOL_OK)
  {
    uint count;
    DYNAMIC_STRING json;
    MYSQL_LEX_STRING *names;
    DYNAMIC_COLUMN_VALUE *vals;

    mariadb_dyncol_column_count(&col, &count);
    if (mariadb_dyncol_unpack(&col, &count, &names, &vals) == ER_DYNCOL_OK)
    {
      for (uint i= 0; i < count; i++)
      {
        DYNAMIC_STRING s;
        init_dynamic_string(&s, NULL, 0, 0);
        mariadb_dyncol_val_str(&s, &vals[i], &my_charset_utf8mb4_general_ci, 1);
        dynstr_free(&s);
      }
      mariadb_dyncol_unpack_free(names, vals);
    }
    init_dynamic_string(&json, NULL, 0, 0);
    mariadb_dyncol_json(&col, &json);
    dynstr_free(&json);
    if (mariadb_dyncol_has_names(&col))
    {
      MYSQL_LEX_STRING *lst;
      if (mariadb_dyncol_list_named(&col, &count, &lst) == ER_DYNCOL_OK)
        my_free(lst);
    }
    else
    {
      uint *nums;
      if (mariadb_dyncol_list_num(&col, &count, &nums) == ER_DYNCOL_OK)
        my_free(nums);
    }
  }
  free(col.str);
  return 0;
}
