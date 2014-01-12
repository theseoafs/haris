#include "cgenh.h"
#include "cgen.h"

static CJobStatus write_header_boilerplate(CJob *);
static CJobStatus write_header_macros(CJob *);
static CJobStatus write_header_structures(CJob *);
static CJobStatus write_reflective_structures(CJob *);
static CJobStatus write_structure_definition(CJob *, ParsedStruct *);
static CJobStatus write_child_field(CJob *, const ChildField *);

/* =============================PUBLIC INTERFACE============================= */

CJobStatus write_header_file(CJob *job)
{
  CJobStatus result;
  if ((result = write_header_boilerplate(job)) != CJOB_SUCCESS)
    return result;
  if ((result = write_header_macros(job)) != CJOB_SUCCESS)
    return result;
  if ((result = write_header_structures(job)) != CJOB_SUCCESS)
    return result;
  return CJOB_SUCCESS;
}

/* =============================STATIC FUNCTIONS============================= */

/* For now, the "boilerplate" section of the header will just contain
   the "includes" that the source file is going to require. As we add more
   protocols, this function may need to be edited to reflect those changes.
   For example, running the socket protocol would require that we #include
   the header file for dealing with sockets.
*/
static CJobStatus write_header_boilerplate(CJob *job)
{
  CJOB_FMT_HEADER_STRING(job, 
"#include <stdio.h>\n\
#include <stdlib.h>\n\
#include <stddef.h>\n\
#include <string.h>\n\n");
  CJOB_FMT_HEADER_STRING(job, 
"/* In order to generate C code, the utility library needs exact-precision\n\
   unsigned and signed integers. In particular, in order to ensure that we don't\n\
   overflow the native integer containers when parsing Haris messages, we need\n\
   to be certain about the minimum size of our integers. Haris defines the\n\
   typedefs\n\
   haris_intN_t\n\
   and\n\
   haris_uintN_t\n\
   for N in [8, 16, 32, 64]. The intN types must be signed, and the uintN types\n\
   must be unsigned. Each type must have at least N bits (that is, haris_int8_t\n\
   must have at least 8 bits and must be signed). If your system includes \n\
   stdint.h (which it will if you have a standard-conforming C99 compiler), then\n\
   the typedef's automatically generated by the code generator should be \n\
   sufficient, and you should be able to include the generated files in your \n\
   project without changing them. If you do not have stdint.h, then you'll have\n\
   to manually modify the following 8 typedef's yourself (though that shouldn't\n\
   take more than a minute to do). Make sure to remove the #include directive \n\
   if you do not have stdint.h.\n\
\n\
   These type definitions trade time for space; that is, they use the fastest\n\
   possible types with those sizes rather than the smallest. This means that\n\
   the in-memory representation of a structure might be larger than is \n\
   technically necessary to store the number. If you wish to use less space\n\
   in-memory in exchange for a potentially longer running time, use the\n\
   [u]int_leastN_t types rather than the [u]int_fastN_t types.\n*/\n");
  CJOB_FMT_HEADER_STRING(job,
"#include <stdint.h>\n\
\n\
typedef uint_fast8_t    haris_uint8_t;\n\
typedef int_fast8_t     haris_int8_t;\n\
typedef uint_fast16_t   haris_uint16_t;\n\
typedef int_fast16_t    haris_int16_t;\n\
typedef uint_fast32_t   haris_uint32_t;\n\
typedef int_fast32_t    haris_int32_t;\n\
typedef uint_fast64_t   haris_uint64_t;\n\
typedef int_fast64_t    haris_int64_t;\n\
\n\
typedef float           haris_float32;\n\
typedef double          haris_float64;\n\
\n\
typedef enum {\n\
  HARIS_SUCCESS, HARIS_STRUCTURE_ERROR, HARIS_DEPTH_ERROR, HARIS_SIZE_ERROR,\n\
  HARIS_INPUT_ERROR, HARIS_MEM_ERROR\n\
} HarisStatus;\n\n\
typedef HarisStatus (*HarisStreamReader)(void *, haris_uint32_t, \n\
                                         const unsigned char **);\n\n\
typedef HarisStatus (*HarisStreamWriter)(void *, const unsigned char *, \n\
                                         haris_uint32_t);\n\n");
  return CJOB_SUCCESS;
}

/* We need to define macros for every structure and enumeration in the
   schema. For an enumeration E with a value V (and assuming a prefix P), the 
   generated enumerated name is
   PE_V

   For every structure in C, we define macros to give us 1) the number of
   bytes in the body and 2) the number of children we expect to have
   in each of these structures. The number of bytes in the body is defined
   for a structure S as
   S_LIB_BODY_SZ
   and the number of children is defined as
   S_LIB_NUM_CHILDREN
*/
static CJobStatus write_header_macros(CJob *job)
{
  int i, j;
  ParsedEnum *enm;
  ParsedStruct *strct;
  ChildField *child;
  const char *prefix = job->prefix, *strct_name, *child_name;
  CJOB_FMT_HEADER_STRING(job, 
"/* Changeable size limits for error-checking. You can freely modify these if\n\
   you would like your Haris client to be able to process larger or deeper\n\
   messages. \n\
*/\n\
\n\
#define HARIS_DEPTH_LIMIT 64\n\
#define HARIS_MESSAGE_SIZE_LIMIT 1000000000\n\
\n\
/* \"Magic numbers\" for use by float-reading and -writing functions; do not \n\
   modify\n\
*/\n\
\n\
#define HARIS_FLOAT32_SIGBITS 23\n\
#define HARIS_FLOAT32_BIAS    127\n\
#define HARIS_FLOAT64_SIGBITS 52\n\
#define HARIS_FLOAT64_BIAS    1023\n\n\
/* The _init_ deallocation factor. If you initialize a list to have length\n\
   N, but the list is already allocated to have length A, then the list\n\
   will be reallocated to have length N if and only if N/A is less than\n\
   the deallocation factor. The deallocation factor must be between 0.0 and\n\
   1.0; lower values will waste more memory but will not interface with the\n\
   memory allocator as much, and higher values will waste less memory but\n\
   will have to reallocate more.\n\
*/\n\n\
#define HARIS_DEALLOC_FACTOR 0.6\n\n\
/* The drop-in memory management functions. If you want, you can use a\n\
   custom memory allocator, rather than just using the standard library's.\n\
   A custom allocator needs to implement a function that works like malloc\n\
   (HARIS_MALLOC), a function that works like realloc (HARIS_REALLOC), \n\
   and a function that works like free (HARIS_FREE).\n\
*/\n\n\
#define HARIS_MALLOC(n) malloc(n)\n\
#define HARIS_REALLOC(p, n) realloc((p), (n))\n\
#define HARIS_FREE(p) free(p)\n\
\n\
#define HARIS_ASSERT(cond, err) if (!(cond)) return HARIS_ ## err ## _ERROR\n\n");
  for (i = 0; i < job->schema->num_structs; i ++) {
    strct = &job->schema->structs[i];
    strct_name = strct->name;
    for (j = 0; j < strct->num_children; j ++) {
      child = &strct->children[j];
      child_name = child->name;
      if (child->nullable) {
        CJOB_FMT_HEADER_STRING(job, 
                               "#define %s%s_null_%s(X) ((int)((X)->_%s_info.null))\n",
                               prefix, strct_name, child_name, child_name);
        CJOB_FMT_HEADER_STRING(job,
                               "#define %s%s_nullify_%s(X) ((X)->_%s_info.null = 1)\n",
                               prefix, strct_name, child_name, child_name);
      }
      if (child->tag != CHILD_STRUCT) {
        CJOB_FMT_HEADER_STRING(job, 
                               "#define %s%s_len_%s(X) \
((haris_uint32_t)((X)->_%s_info.len))\n",
                               prefix, strct_name, child_name, child_name);
      }
      CJOB_FMT_HEADER_STRING(job, "#define %s%s_get_%s(X) ",
                             prefix, strct_name, child_name);
      switch (child->tag) {
      case CHILD_TEXT:
        CJOB_FMT_HEADER_STRING(job, "((char*)");
        break;
      case CHILD_SCALAR_LIST:
        CJOB_FMT_HEADER_STRING(job, "((%s*)", 
                               scalar_type_name(child->type.scalar_list.tag));
        break;
      case CHILD_STRUCT_LIST:
        CJOB_FMT_HEADER_STRING(job, "((%s%s*)",
                               prefix, child->type.struct_list->name);
        break;
      case CHILD_STRUCT:
        CJOB_FMT_HEADER_STRING(job, "((%s%s*)", prefix, 
                               child->type.strct->name);
        break;
      }
      CJOB_FMT_HEADER_STRING(job, "((X)->_%s_info.ptr))\n\n", child_name);
    }
  }
  for (i = 0; i < job->schema->num_enums; i ++) {
    enm = &job->schema->enums[i];
    CJOB_FMT_HEADER_STRING(job, "/* enum %s */\n", enm->name);
    for (j = 0; j < job->schema->enums[i].num_values; j ++) {
      CJOB_FMT_HEADER_STRING(job, "#define %s%s_%s %d\n", 
                  job->prefix, enm->name, enm->values[j], j);
    }
    CJOB_FMT_HEADER_STRING(job, "\n");
  }
  return CJOB_SUCCESS;
}


/* Write the generic structures that capture the makeup of the defined 
   structures.
*/
static CJobStatus write_reflective_structures(CJob *job)
{
  CJOB_FMT_HEADER_STRING(job, 
"typedef enum {\n\
  HARIS_SCALAR_UINT8, HARIS_SCALAR_INT8, HARIS_SCALAR_UINT16,\n\
  HARIS_SCALAR_INT16, HARIS_SCALAR_UINT32, HARIS_SCALAR_INT32,\n\
  HARIS_SCALAR_UINT64, HARIS_SCALAR_INT64, HARIS_SCALAR_FLOAT32,\n\
  HARIS_SCALAR_FLOAT64, HARIS_SCALAR_BLANK\n\
} HarisScalarType;\n\n");
  CJOB_FMT_HEADER_STRING(job, 
"typedef enum {\n\
  HARIS_CHILD_TEXT, HARIS_CHILD_SCALAR_LIST, HARIS_CHILD_STRUCT_LIST,\n\
  HARIS_CHILD_STRUCT\n\
} HarisChildType;\n\n");
  CJOB_FMT_HEADER_STRING(job, 
"typedef struct {\n\
  void *         ptr;\n\
  haris_uint32_t len;\n\
  haris_uint32_t alloc;\n\
  char           null;\n\
} HarisListInfo;\n\n");
  CJOB_FMT_HEADER_STRING(job,
"typedef struct {\n\
  void *ptr;\n\
  char null;\n\
} HarisSubstructInfo;\n\n");
  CJOB_FMT_HEADER_STRING(job, 
"typedef struct HarisStructureInfo_ HarisStructureInfo;\n\n");
  CJOB_FMT_HEADER_STRING(job, 
"typedef struct {\n\
  size_t offset;\n\
  HarisScalarType type;\n\
} HarisScalar;\n\n");
  CJOB_FMT_HEADER_STRING(job, 
"typedef struct {\n\
  size_t offset;\n\
  int nullable;\n\
  HarisScalarType scalar_element;\n\
  const HarisStructureInfo *struct_element;\n\
  HarisChildType child_type;\n\
} HarisChild;\n\n");
  CJOB_FMT_HEADER_STRING(job, 
"struct HarisStructureInfo_ {\n\
  int num_scalars;\n\
  const HarisScalar *scalars;\n\
  int num_children;\n\
  const HarisChild *children;\n\
  int body_size;\n\
  size_t size_of;\n\
};\n\n");
  return CJOB_SUCCESS;
}

/* We need to make two passes through the structure array to define 
   our structures. First, for every structure S (and assuming a prefix P), 
   we do
     typedef struct haris_PS PS;
   ... which has the dual effect of performing the typedef and "forward-
   declaring" the structure so that we don't get any nasty compile errors.
   Then, we loop through the structures again and define them. 
*/
static CJobStatus write_header_structures(CJob *job)
{
  CJobStatus result;
  int i;
  if ((result = write_reflective_structures(job)) != CJOB_SUCCESS)
    return result;

  for (i = 0; i < job->schema->num_structs; i ++) {
    if ((result = write_structure_definition(job, job->schema->structs + i))
        != CJOB_SUCCESS)
      return result;
  }

  return CJOB_SUCCESS;
}

static CJobStatus write_structure_definition(CJob *job, ParsedStruct *strct)
{
  CJobStatus result;
  int i;
  unsigned j;
  const char *prefix = job->prefix, *name = strct->name;
  static const ScalarTag scalars_by_size[] = {
    SCALAR_UINT64, SCALAR_INT64, SCALAR_FLOAT64, SCALAR_UINT32, SCALAR_INT32, 
    SCALAR_FLOAT32, SCALAR_UINT16, SCALAR_INT16, SCALAR_BOOL, SCALAR_ENUM, 
    SCALAR_UINT8, SCALAR_INT8
  };
  CJOB_FMT_HEADER_STRING(job, "typedef struct {\n");
  for (i = 0; i < strct->num_children; i ++) { 
    if ((result = write_child_field(job, strct->children + i)) != CJOB_SUCCESS)
      return result;
  }
  for (j = 0; j < sizeof scalars_by_size / sizeof scalars_by_size[0]; j ++) {
    for (i = 0; i < strct->num_scalars; i ++) {
      if (strct->scalars[i].type.tag == scalars_by_size[j]) {
        CJOB_FMT_HEADER_STRING(job, "  %s %s;\n",
                               scalar_type_name(strct->scalars[i].type.tag),
                               strct->scalars[i].name);
      }
    }
  }
  CJOB_FMT_HEADER_STRING(job, "} %s%s;\n\n", prefix, name);
  return CJOB_SUCCESS;
}

/* Write a child field to the output file. */
static CJobStatus write_child_field(CJob *job, const ChildField *child)
{
  const char *child_name = child->name;
  switch (child->tag) {
  case CHILD_TEXT:
  case CHILD_SCALAR_LIST:
  case CHILD_STRUCT_LIST:
    CJOB_FMT_HEADER_STRING(job, "  HarisListInfo _%s_info;\n", 
                           child_name);
    break;
  case CHILD_STRUCT:
    CJOB_FMT_HEADER_STRING(job, "  HarisSubstructInfo _%s_info;\n", 
                           child_name);
    break;
  }
  return CJOB_SUCCESS;
}
