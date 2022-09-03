#ifndef ____PROPOSALS_GV__H__
#define ____PROPOSALS_GV__H__
/* generated code for proposals.gv */
#include <string.h>
#include <glib.h>

/********** Basic types *****************/

typedef struct {
 gconstpointer base;
 gsize size;
} Ref;

typedef struct {
 gconstpointer base;
 gsize size;
} VariantRef;

#define PROPOSAL_TYPESTRING "a{sv}"
#define PROPOSAL_TYPEFORMAT ((const GVariantType *) PROPOSAL_TYPESTRING)

typedef struct {
 gconstpointer base;
 gsize size;
} ProposalRef;

typedef struct {
 gconstpointer base;
 gsize size;
} ProposalEntryRef;

#define RESULTS_TYPESTRING "aa{sv}"
#define RESULTS_TYPEFORMAT ((const GVariantType *) RESULTS_TYPESTRING)

typedef struct {
 gconstpointer base;
 gsize size;
} ResultsRef;

#define CHUNK_TYPESTRING "a{sv}"
#define CHUNK_TYPEFORMAT ((const GVariantType *) CHUNK_TYPESTRING)

typedef struct {
 gconstpointer base;
 gsize size;
} ChunkRef;

typedef struct {
 gconstpointer base;
 gsize size;
} ChunkEntryRef;

#define CHUNKS_TYPESTRING "aa{sv}"
#define CHUNKS_TYPEFORMAT ((const GVariantType *) CHUNKS_TYPESTRING)

typedef struct {
 gconstpointer base;
 gsize size;
} ChunksRef;

#endif
