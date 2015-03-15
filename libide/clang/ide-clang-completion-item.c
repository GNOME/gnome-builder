/* ide-clang-completion-item.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <clang-c/Index.h>
#include <glib/gi18n.h>
#include <gtksourceview/gtksourcecompletionproposal.h>

#include "ide-clang-completion-item.h"
#include "ide-ref-ptr.h"

struct _IdeClangCompletionItemClass
{
  GObjectClass parent_class;
};

struct _IdeClangCompletionItem
{
  GObject    parent_instance;

  IdeRefPtr *results;
  guint      index;
};

enum
{
  PROP_0,
  PROP_INDEX,
  PROP_RESULTS,
  LAST_PROP
};

static void completion_proposal_iface_init (GtkSourceCompletionProposalIface *);

G_DEFINE_TYPE_EXTENDED (IdeClangCompletionItem, ide_clang_completion_item, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (GTK_SOURCE_TYPE_COMPLETION_PROPOSAL,
                                               completion_proposal_iface_init))

static GParamSpec *gParamSpecs [LAST_PROP];

static void
ide_clang_completion_item_finalize (GObject *object)
{
  IdeClangCompletionItem *self = (IdeClangCompletionItem *)object;

  g_clear_pointer (&self->results, ide_ref_ptr_unref);

  G_OBJECT_CLASS (ide_clang_completion_item_parent_class)->finalize (object);
}

static void
ide_clang_completion_item_set_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  IdeClangCompletionItem *self = IDE_CLANG_COMPLETION_ITEM (object);

  switch (prop_id)
    {
    case PROP_INDEX:
      self->index = g_value_get_uint (value);
      break;

    case PROP_RESULTS:
      self->results = g_value_dup_boxed (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_clang_completion_item_class_init (IdeClangCompletionItemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_clang_completion_item_finalize;
  object_class->set_property = ide_clang_completion_item_set_property;

  gParamSpecs [PROP_INDEX] =
    g_param_spec_uint ("index",
                       "Index",
                       "The index in the result structure.",
                       0,
                       G_MAXUINT,
                       0,
                       (G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_INDEX, gParamSpecs [PROP_INDEX]);

  gParamSpecs [PROP_RESULTS] =
    g_param_spec_boxed ("results",
                        "Results",
                        "The result set from clang.",
                        IDE_TYPE_REF_PTR,
                        (G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_RESULTS, gParamSpecs [PROP_RESULTS]);
}

static void
ide_clang_completion_item_init (IdeClangCompletionItem *item)
{
}

static CXCompletionResult *
get_completion_result (GtkSourceCompletionProposal *proposal)
{
  IdeClangCompletionItem *self = (IdeClangCompletionItem *)proposal;
  CXCodeCompleteResults *results;
  CXCompletionResult *result;

  results = ide_ref_ptr_get (self->results);
  result = &results->Results [self->index];

  return result;
}

static gchar *
ide_clang_completion_item_get_label (GtkSourceCompletionProposal *proposal)
{
  CXCompletionResult *result = get_completion_result (proposal);
  GString *str;
  unsigned num_chunks;
  unsigned i;

  str = g_string_new (NULL);
  num_chunks = clang_getNumCompletionChunks (result->CompletionString);

  for (i = 0; i < num_chunks; i++)
    {
      CXString cxstr;

      cxstr = clang_getCompletionChunkText (result->CompletionString, i);

      if (str->len)
        g_string_append_printf (str, " %s", clang_getCString (cxstr));
      else
        g_string_append (str, clang_getCString (cxstr));
    }

  return g_string_free (str, FALSE);
}

static void
completion_proposal_iface_init (GtkSourceCompletionProposalIface *iface)
{
  iface->get_label = ide_clang_completion_item_get_label;
}

gint
ide_clang_completion_item_sort (gconstpointer a,
                                gconstpointer b)
{
  CXCompletionResult *ar = get_completion_result ((gpointer)a);
  CXCompletionResult *br = get_completion_result ((gpointer)b);
  unsigned aprio;
  unsigned bprio;

  aprio = clang_getCompletionPriority (ar->CompletionString);
  bprio = clang_getCompletionPriority (br->CompletionString);

  /* TODO: check that this is safe */

  return (gint)aprio - (gint)bprio;
}
