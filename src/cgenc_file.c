#include "cgenc_file.h"

static CJobStatus write_file_structures(CJob *);
static CJobStatus write_public_file_funcs(CJob *, ParsedStruct *);
static CJobStatus write_static_file_funcs(CJob *);

/* =============================PUBLIC INTERFACE============================= */

/* Writes the file protocol functions to the output source stream. They are
   HarisStatus S_from_file(S *, FILE *);
   HarisStatus S_to_file(S *, FILE *);
   static HarisStatus _S_from_file(S *, FILE *, haris_uint32_t *, int);
   static HarisStatus _S_to_file(S *, FILE *);
   static int handle_child_file(FILE *, int);
*/
CJobStatus write_file_protocol_funcs(CJob *job)
{
  CJobStatus result;
  int i;
  ParsedSchema *schema = job->schema;
  if ((result = write_file_structures(job)) != CJOB_SUCCESS ||
      (result = write_static_file_funcs(job)) != CJOB_SUCCESS)
    return result;
  for (i = 0; i < schema->num_structs; i ++) {
    if ((result = write_public_file_funcs(job, &schema->structs[i])) 
        != CJOB_SUCCESS)
      return result;
  }
  return CJOB_SUCCESS;
}

/* =============================STATIC FUNCTIONS============================= */

static CJobStatus write_file_structures(CJob *job)
{
  CJOB_FMT_HEADER_STRING(job,
"typedef struct {\n\
  FILE *file;\n\
  haris_uint32_t curr;\n\
  unsigned char buffer[256];\n\
} HarisFileStream;\n\n");
  return CJOB_SUCCESS;
}

static CJobStatus write_static_file_funcs(CJob *job)
{
  CJOB_FMT_PRIV_FUNCTION(job,
"static HarisStatus read_from_file_stream(void *_stream,\n\
                                         haris_uint32_t count,\n\
                                         const unsigned char **dest)\n\
{\n\
  HarisFileStream *stream = (HarisFileStream*)_stream;\n\
  HARIS_ASSERT(count + stream->curr <= HARIS_MESSAGE_SIZE_LIMIT, SIZE);\n\
  HARIS_ASSERT(count <= 256, SIZE);\n\
  HARIS_ASSERT(fread(stream->buffer, 1, count, stream->file) == count,\n\
               INPUT);\n\
  *dest = stream->buffer;\n\
  stream->curr = count;\n\
  return HARIS_SUCCESS;\n\
}\n\n");
  CJOB_FMT_PRIV_FUNCTION(job,
"static HarisStatus write_to_file_stream(void *_stream,\n\
                                        const unsigned char *src,\n\
                                        haris_uint32_t count)\n\
{\n\
  HarisFileStream *stream = (HarisFileStream*)_stream;\n\
  HARIS_ASSERT(fwrite(src, 1, count, stream->file) == count, INPUT);\n\
  stream->curr += count;\n\
  return HARIS_SUCCESS;\n\
}\n\n");
  CJOB_FMT_PRIV_FUNCTION(job,
"static HarisStatus _public_to_file(void *ptr,\n\
                                   const HarisStructureInfo *info,\n\
                                   FILE *f,\n\
                                   haris_uint32_t *out_sz)\n\
{\n\
  HarisStatus result;\n\
  HarisFileStream file_stream;\n\
  haris_uint32_t encoded_size = haris_lib_size(ptr, info, 0, &result);\n\
  if (encoded_size == 0) return result;\n\
  HARIS_ASSERT(encoded_size <= HARIS_MESSAGE_SIZE_LIMIT, SIZE);\n\
  file_stream.file = f;\n\
  file_stream.curr = 0;\n\
  if ((result = _haris_to_stream(ptr, info, &file_stream,\n\
                                 write_to_file_stream)) != HARIS_SUCCESS)\n\
    return result;\n\
  if (out_sz) *out_sz = file_stream.curr;\n\
  return HARIS_SUCCESS;\n\
}\n\n");
  CJOB_FMT_PRIV_FUNCTION(job,
"static HarisStatus _public_from_file(void *ptr,\n\
                                     const HarisStructureInfo *info,\n\
                                     FILE *f,\n\
                                     haris_uint32_t *out_sz)\n\
{\n\
  HarisStatus result;\n\
  HarisFileStream file_stream;\n\
  file_stream.file = f;\n\
  file_stream.curr = 0;\n\
  if ((result = _haris_from_stream(ptr, info, &file_stream,\n\
                                   read_from_file_stream, 0)) != HARIS_SUCCESS)\n\
    return result;\n\
  if (out_sz) *out_sz = file_stream.curr;\n\
  return HARIS_SUCCESS;\n\
}\n\n");
  return CJOB_SUCCESS;
}

static CJobStatus write_public_file_funcs(CJob *job, ParsedStruct *strct)
{
  const char *prefix = job->prefix, *name = strct->name;
  CJOB_FMT_PUB_FUNCTION(job,
"HarisStatus %s%s_to_file(%s%s *strct, FILE *f, \n\
                          haris_uint32_t *out_sz)\n\
{\n\
  return _public_to_file(strct, &haris_lib_structures[%d],\n\
                         f, out_sz);\n}\n\n",
                        prefix, name, prefix, name, strct->schema_index);
  CJOB_FMT_PUB_FUNCTION(job,
"HarisStatus %s%s_from_file(%s%s *strct, FILE *f,\n\
                            haris_uint32_t *out_sz)\n\
{\n\
  return _public_from_file(strct, &haris_lib_structures[%d],\n\
                           f, out_sz);\n}\n\n",
                        prefix, name, prefix, name, strct->schema_index);
  return CJOB_SUCCESS;
}

