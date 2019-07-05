#pragma once

#include <glib.h>

#include <libide-docs.h>

G_BEGIN_DECLS

typedef struct _Devhelp2Parser Devhelp2Parser;
typedef struct _Chapter Chapter;
typedef struct _Keyword Keyword;

struct _Chapter
{
  struct _Chapter *parent;
  GQueue           children;
  GList            list;
  const gchar     *name;
  const gchar     *link;
};

struct _Keyword
{
  Devhelp2Parser  *backptr;
  GList            list;
  const gchar     *type;
  const gchar     *name;
  const gchar     *link;
  const gchar     *since;
  const gchar     *deprecated;
  const gchar     *stability;
  IdeDocsItemKind  kind;
};

struct _Devhelp2Parser
{
  GMarkupParseContext *context;
  GStringChunk        *strings;
  GHashTable          *kinds;
  Chapter             *chapter;
  GArray              *keywords;
  gchar               *directory;
  struct {
    const gchar       *title;
    const gchar       *link;
    const gchar       *author;
    const gchar       *name;
    const gchar       *version;
    const gchar       *language;
    const gchar       *online;
  } book;
};

Devhelp2Parser *devhelp2_parser_new        (void);
void            devhelp2_parser_free       (Devhelp2Parser *state);
gboolean        devhelp2_parser_parse_file (Devhelp2Parser  *state,
                                            const gchar     *filename,
                                            GError         **error);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (Devhelp2Parser, devhelp2_parser_free)

G_END_DECLS
