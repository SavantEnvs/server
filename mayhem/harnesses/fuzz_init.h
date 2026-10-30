#ifndef MAYHEM_FUZZ_INIT_H
#define MAYHEM_FUZZ_INIT_H
#include <my_global.h>
#include <my_sys.h>

/* mysys keeps errno and the per-thread state in a thread var that my_init() creates. */
int LLVMFuzzerInitialize(int *argc, char ***argv)
{
  (void) argc;
  (void) argv;
  MY_INIT("fuzz");
  return 0;
}
#endif
