/*
  strings/decimal.c: DECIMAL parsing, arithmetic, rounding and binary round trip.
  Input: two decimal texts separated by the first newline, with the first byte
  selecting rounding scale and mode.
*/
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <my_global.h>
#include <my_sys.h>
#include <decimal.h>
#include "fuzz_init.h"

#define BUFLEN 9

static void init(decimal_t *d, decimal_digit_t *buf)
{
  d->buf= buf;
  d->len= BUFLEN;
  d->intg= d->frac= 0;
  d->sign= 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
  if (size < 2)
    return -1;
  int scale= (int) (data[0] & 31) - 8;
  decimal_round_mode mode= (decimal_round_mode) (data[0] >> 5) % 5;
  data++;
  size--;

  char *text= (char *) malloc(size + 1);
  memcpy(text, data, size);
  text[size]= '\0';
  char *nl= (char *) memchr(text, '\n', size);
  const char *second= "1";
  if (nl)
  {
    *nl= '\0';
    second= nl + 1;
  }

  decimal_digit_t b1[BUFLEN]= {0}, b2[BUFLEN]= {0}, br[BUFLEN]= {0}, bt[BUFLEN]= {0};
  decimal_t a, b, r, t;
  char *end;
  init(&a, b1); init(&b, b2); init(&r, br); init(&t, bt);

  end= text + strlen(text);
  int ea= string2decimal(text, &a, &end);
  end= (char *) second + strlen(second);
  int eb= string2decimal(second, &b, &end);

  char out[128];
  int out_len;
  if (ea <= E_DEC_TRUNCATED)
  {
    out_len= sizeof(out);
    decimal2string(&a, out, &out_len, 0, 0, 0);
    out_len= sizeof(out);
    decimal2string(&a, out, &out_len, 20, 5, '0');
    double dv; longlong ll; ulonglong ull;
    decimal2double(&a, &dv);
    decimal2longlong(&a, &ll, mode);
    decimal2ulonglong(&a, &ull, mode);
    decimal_round(&a, &r, scale, mode);

    int prec= a.intg + a.frac;
    if (prec >= 1 && prec <= 65 && a.frac <= 30)
    {
      uchar bin[64];
      decimal2bin(&a, bin, prec, a.frac);
      bin2decimal(bin, &t, prec, a.frac);
    }
    init(&t, bt);
    double2decimal(dv, &t);
  }
  if (ea <= E_DEC_TRUNCATED && eb <= E_DEC_TRUNCATED)
  {
    decimal_cmp(&a, &b);
    decimal_add(&a, &b, &r);
    decimal_sub(&a, &b, &r);
    decimal_mul(&a, &b, &r);
    decimal_div(&a, &b, &r, 5);
    decimal_mod(&a, &b, &r);
  }
  free(text);
  return 0;
}
