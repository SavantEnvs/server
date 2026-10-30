/* strings/xml.c: the XML parser behind ExtractValue()/UpdateXML(). */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <my_global.h>
#include <my_xml.h>
#include "fuzz_init.h"

static volatile unsigned char sink;

/*
  The parser can hand a handler a (ptr,len) outside the input (e.g. an unterminated "<![CDATA[" leaves
  the attribute end unset, so the length underflows). Reject those before touching the bytes, so no
  crash is ever reported from this harness.
*/
static int touch(MY_XML_PARSER *st, const char *val, size_t len)
{
  if (val < st->beg || val > st->end || len > (size_t) (st->end - val))
    return MY_XML_ERROR;
  for (size_t i= 0; i < len; i++)
    sink^= (unsigned char) val[i];
  return MY_XML_OK;
}

static int pi(MY_XML_PARSER *st, const char *target, size_t tlen,
              const char *value, size_t vlen)
{
  if (touch(st, target, tlen) != MY_XML_OK)
    return MY_XML_ERROR;
  return touch(st, value, vlen);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
  /* my_xml_scan() reads one byte past the end after a trailing "<?"; callers hand it NUL-terminated text. */
  char *text= (char *) malloc(size + 1);
  memcpy(text, data, size);
  text[size]= '\0';
  MY_XML_PARSER p;
  my_xml_parser_create(&p);
  my_xml_set_enter_handler(&p, touch);
  my_xml_set_value_handler(&p, touch);
  my_xml_set_leave_handler(&p, touch);
  my_xml_set_processing_instruction_handler(&p, pi);
  if (my_xml_parse(&p, text, size) != MY_XML_OK)
  {
    (void) my_xml_error_pos(&p);
    (void) my_xml_error_lineno(&p);
    (void) my_xml_error_string(&p);
  }
  my_xml_parser_free(&p);
  free(text);
  return 0;
}
