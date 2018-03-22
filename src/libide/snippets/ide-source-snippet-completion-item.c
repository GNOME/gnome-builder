/* ide-source-snippet-completion-item.c
 *
 * Copyright 2013 Christian Hergert <christian@hergert.me>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <gio/gio.h>
#include <glib/gi18n.h>

#include "snippets/ide-source-snippet-completion-item.h"

struct _IdeSourceSnippetCompletionItem
{
  GObject           parent_instance;

  IdeSourceSnippet *snippet;
};

enum {
  PROP_0,
  PROP_SNIPPET,
  LAST_PROP
};

static GParamSpec *properties [LAST_PROP];

static void init_proposal_iface (GtkSourceCompletionProposalIface *iface);

G_DEFINE_TYPE_WITH_CODE (IdeSourceSnippetCompletionItem,
                        ide_source_snippet_completion_item,
                        G_TYPE_OBJECT,
                        G_IMPLEMENT_INTERFACE (GTK_SOURCE_TYPE_COMPLETION_PROPOSAL,
                                               init_proposal_iface))

GtkSourceCompletionProposal *
ide_source_snippet_completion_item_new (IdeSourceSnippet *snippet)
{
  g_return_val_if_fail (!snippet || IDE_IS_SOURCE_SNIPPET (snippet), NULL);

  return g_object_new (IDE_TYPE_SOURCE_SNIPPET_COMPLETION_ITEM,
                       "snippet", snippet,
                       NULL);
}

IdeSourceSnippet *
ide_source_snippet_completion_item_get_snippet (IdeSourceSnippetCompletionItem *item)
{
  g_return_val_if_fail (IDE_IS_SOURCE_SNIPPET_COMPLETION_ITEM (item), NULL);
  return item->snippet;
}

void
ide_source_snippet_completion_item_set_snippet (IdeSourceSnippetCompletionItem *item,
                                               IdeSourceSnippet               *snippet)
{
  g_return_if_fail (IDE_IS_SOURCE_SNIPPET_COMPLETION_ITEM (item));
  g_clear_object (&item->snippet);
  item->snippet = g_object_ref (snippet);
}

static void
ide_source_snippet_completion_item_finalize (GObject *object)
{
  IdeSourceSnippetCompletionItem *self = IDE_SOURCE_SNIPPET_COMPLETION_ITEM (object);

  g_clear_object (&self->snippet);

  G_OBJECT_CLASS (ide_source_snippet_completion_item_parent_class)->finalize (object);
}

static void
ide_source_snippet_completion_item_get_property (GObject    *object,
                                                guint       prop_id,
                                                GValue     *value,
                                                GParamSpec *pspec)
{
  IdeSourceSnippetCompletionItem *item = IDE_SOURCE_SNIPPET_COMPLETION_ITEM (object);

  switch (prop_id)
    {
    case PROP_SNIPPET:
      g_value_set_object (value, ide_source_snippet_completion_item_get_snippet (item));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_source_snippet_completion_item_set_property (GObject      *object,
                                                guint         prop_id,
                                                const GValue *value,
                                                GParamSpec   *pspec)
{
  IdeSourceSnippetCompletionItem *item = IDE_SOURCE_SNIPPET_COMPLETION_ITEM (object);

  switch (prop_id)
    {
    case PROP_SNIPPET:
      ide_source_snippet_completion_item_set_snippet (item, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_source_snippet_completion_item_class_init (IdeSourceSnippetCompletionItemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_source_snippet_completion_item_finalize;
  object_class->get_property = ide_source_snippet_completion_item_get_property;
  object_class->set_property = ide_source_snippet_completion_item_set_property;

  properties[PROP_SNIPPET] =
    g_param_spec_object ("snippet",
                         "Snippet",
                         "The snippet to insert.",
                         IDE_TYPE_SOURCE_SNIPPET,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
ide_source_snippet_completion_item_init (IdeSourceSnippetCompletionItem *item)
{
}

static gchar *
get_label (GtkSourceCompletionProposal *p)
{
  IdeSourceSnippetCompletionItem *item = IDE_SOURCE_SNIPPET_COMPLETION_ITEM (p);
  const gchar *trigger = NULL;
  const gchar *description = NULL;

  if (item->snippet)
    {
      trigger = ide_source_snippet_get_trigger (item->snippet);
      description = ide_source_snippet_get_description (item->snippet);
    }

  if (description) 
    return g_strdup_printf ("%s: %s", trigger, description);
  else
    return g_strdup(trigger);
}

static GdkPixbuf *
get_icon (GtkSourceCompletionProposal *proposal)
{
  /*
   * TODO: Load pixbufs from resources, assign based on completion type.
   */

  return NULL;
}

static void
init_proposal_iface (GtkSourceCompletionProposalIface *iface)
{
  iface->get_label = get_label;
  iface->get_icon = get_icon;
}
