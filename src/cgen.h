#ifndef CGEN_H_
#define CGEN_H_

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdarg.h>
#include "schema.h"

#define CJOB_FPRINTF(...) do { if (fprintf(__VA_ARGS__) < 0) \
                                 return CJOB_IO_ERROR; } while (0)
#define CJOB_FMT_HEADER_STRING(job, ...) do \
                          { if (add_header_string(job, strformat(__VA_ARGS__)) \
                                 != CJOB_SUCCESS) \
                              return CJOB_MEM_ERROR; } while (0)
#define CJOB_FMT_HEADER_BOTTOM_STRING(job, ...) do \
                          { if (add_header_bottom_string(job, \
                                strformat(__VA_ARGS__)) != CJOB_SUCCESS) \
                              return CJOB_MEM_ERROR; } while (0)
#define CJOB_FMT_SOURCE_STRING(job, ...) do \
                          { if (add_source_string(job, strformat(__VA_ARGS__)) \
                                 != CJOB_SUCCESS) \
                              return CJOB_MEM_ERROR; } while (0)
#define CJOB_FMT_PUB_FUNCTION(job, ...) do \
                          { if (add_public_function(job, strformat(__VA_ARGS__)) \
                                 != CJOB_SUCCESS) \
                              return CJOB_MEM_ERROR; } while (0)
#define CJOB_FMT_PRIV_FUNCTION(job, ...) do \
                          { if (add_private_function(job, strformat(__VA_ARGS__)) \
                                 != CJOB_SUCCESS) \
                              return CJOB_MEM_ERROR; } while (0)

/* Main entry point for the C compiler. The main point of interest here is 
   the cgen_main() function, which accepts as its input the `argv` and `argc` 
   that were passed to the main() function. This function processes the 
   command-line arguments and contructs a "CJob", which is a data structure
   used to capture arbitrary compilation jobs requested at the command line.
   If all the command-line options could be successfully parsed, and
   there were no compile-time errors relating to the processing of the schema,
   the CJob is then "run" by a static function called "run_cjob".

   In sum, a compiled Haris library is made up, roughly, of two parts:
   1) The core library. This body of code is largely unchanging, and
   contains functions that can be used to construct in-memory C structures,
   destroy these same in-memory C structures, and convert between these
   structures and small in-memory buffers. In short, the library contains
   the code that's necessary for a Haris runtime to work, no matter what
   protocol we're generating.
   2) The protocol library (libraries). This section builds off of the
   core library, and contains the code that will transmit Haris messages
   along the chosen protocol. 

   Information about the content and implementation of these libraries can
   be found in the relevant headers.
*/

typedef enum {
  CJOB_SUCCESS, CJOB_SCHEMA_ERROR, CJOB_JOB_ERROR, CJOB_IO_ERROR,
  CJOB_MEM_ERROR, CJOB_PARSE_ERROR
} CJobStatus;

typedef struct {
  int num_strings;
  int strings_alloc;
  char **strings;
} CJobStringStack;

/* A structure that is used to organize the output of a C compilation job. 
   In fact, only one function
   actually writes the output to the output files; the rest of the functions
   store strings in this data structure, which are written out to disk later. 
   There are four stacks here; which stack you store a string in decides
   A) which file it is written to. Strings in the header_strings stack are
   written to the header file; the rest are written to the source file.
   B) What, if any, action should be taken with the content of the strings.
   If you store a string in the public_ or private_function stacks, then a 
   prototype will be adapted from the function definition you've given and
   written to the correct place in either the header file or the source file.

   The advantage of using an additional structure is to make it easier to
   extend the compiler or modify its behavior.

   The strings that are stored MUST be dynamically allocated, as they will
   all be `free`d when the time comes to destroy the CJob. In general, you
   should not try to access a string at all once you push it onto the stack;
   that is, if you have a char *s = ..., and you push s onto one of
   the stacks, you should never try to access s again, as it must be assumed
   to be immediately invalidated. The library will free it later. If you must
   continue to access it after pushing it onto the stack, push a copy 
   (that is, push strdup(s)).
*/ 
typedef struct {
  CJobStringStack header_strings; /* Strings that will be copied verbatim into
                                     the header file */
  CJobStringStack header_bottom_strings; /* Strings that will be copied 
                                            verbatim into the header file, but
                                            at the bottom of the file, after
                                            the function declarations */
  CJobStringStack source_strings; /* Strings to copy into the .c file */
  CJobStringStack public_functions; /* Functions that are part of the public
                                       interface of the library */
  CJobStringStack private_functions; /* Functions that are statically 
                                        defined */
} CJobStrings;

typedef struct {
  int buffer;
  int file;
  int fd;
} CJobProtocols;

typedef struct {
  ParsedSchema *schema; /* The schema to be compiled */
  const char *prefix;   /* Prefix all global names with this string */
  const char *output;   /* Write the output code to a file with this name */
  CJobProtocols protocols;
  CJobStrings strings; /* The strings that we will copy into the result source
                          and header files; this is built up dynamically at
                          compile time */
} CJob;

/* The entry point for the C compiler (same arguments as the true main
   function). */
CJobStatus cgen_main(int, char **);

/* A collection of public functions that are used by more than one of the 
   source files of the C compiler. */
CJobStatus add_header_string(CJob *, char *);
CJobStatus add_header_bottom_string(CJob *, char *);
CJobStatus add_source_string(CJob *, char *);
CJobStatus add_public_function(CJob *, char *);
CJobStatus add_private_function(CJob *, char *);

/* A loose wrapper around asprintf; consumes a format string and a set of
   parameters and writes it out to a new dynamically allocated string. 
   Returns either that new string or NULL if there was a memory or format
   error. */
char *strformat(const char *, ...);

int child_is_embeddable(const ChildField *);
int scalar_bit_pattern(ScalarTag type);
int sizeof_scalar(ScalarTag type);
const char *scalar_type_name(ScalarTag);

#endif
