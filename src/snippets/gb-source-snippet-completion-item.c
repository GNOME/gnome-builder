/* gb-source-snippet-completion-item.c
 *
 * Copyright (C) 2013 Christian Hergert <christian@hergert.me>
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

#include "gb-source-snippet-completion-item.h"

struct _GbSourceSnippetCompletionItemPrivate
{
  GbSourceSnippet *snippet;
};

enum {
  PROP_0,
  PROP_SNIPPET,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

static void init_proposal_iface (GtkSourceCompletionProposalIface *iface);

G_DEFINE_TYPE_EXTENDED (GbSourceSnippetCompletionItem,
                        gb_source_snippet_completion_item,
                        G_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE (GTK_SOURCE_TYPE_COMPLETION_PROPOSAL,
                                               init_proposal_iface)
                        G_ADD_PRIVATE (GbSourceSnippetCompletionItem))

GtkSourceCompletionProposal *
gb_source_snippet_completion_item_new (GbSourceSnippet *snippet)
{
  g_return_val_if_fail (!snippet || GB_IS_SOURCE_SNIPPET (snippet), NULL);

  return g_object_new (GB_TYPE_SOURCE_SNIPPET_COMPLETION_ITEM,
                       "snippet", snippet,
                       NULL);
}

GbSourceSnippet *
gb_source_snippet_completion_item_get_snippet (GbSourceSnippetCompletionItem *item)
{
  g_return_val_if_fail (GB_IS_SOURCE_SNIPPET_COMPLETION_ITEM (item), NULL);
  return item->priv->snippet;
}

void
gb_source_snippet_completion_item_set_snippet (GbSourceSnippetCompletionItem *item,
                                               GbSourceSnippet               *snippet)
{
  g_return_if_fail (GB_IS_SOURCE_SNIPPET_COMPLETION_ITEM (item));
  g_clear_object (&item->priv->snippet);
  item->priv->snippet = g_object_ref (snippet);
}

static void
gb_source_snippet_completion_item_finalize (GObject *object)
{
  GbSourceSnippetCompletionItemPrivate *priv;

  priv = GB_SOURCE_SNIPPET_COMPLETION_ITEM (object)->priv;

  g_clear_object (&priv->snippet);

  G_OBJECT_CLASS (gb_source_snippet_completion_item_parent_class)->finalize (object);
}

static void
gb_source_snippet_completion_item_get_property (GObject    *object,
                                                guint       prop_id,
                                                GValue     *value,
                                                GParamSpec *pspec)
{
  GbSourceSnippetCompletionItem *item = GB_SOURCE_SNIPPET_COMPLETION_ITEM (object);

  switch (prop_id)
    {
    case PROP_SNIPPET:
      g_value_set_object (value, gb_source_snippet_completion_item_get_snippet (item));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_source_snippet_completion_item_set_property (GObject      *object,
                                                guint         prop_id,
                                                const GValue *value,
                                                GParamSpec   *pspec)
{
  GbSourceSnippetCompletionItem *item = GB_SOURCE_SNIPPET_COMPLETION_ITEM (object);

  switch (prop_id)
    {
    case PROP_SNIPPET:
      gb_source_snippet_completion_item_set_snippet (item, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_source_snippet_completion_item_class_init (GbSourceSnippetCompletionItemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gb_source_snippet_completion_item_finalize;
  object_class->get_property = gb_source_snippet_completion_item_get_property;
  object_class->set_property = gb_source_snippet_completion_item_set_property;

  gParamSpecs[PROP_SNIPPET] =
    g_param_spec_object ("snippet",
                         _("Snippet"),
                         _("The snippet to insert."),
                         GB_TYPE_SOURCE_SNIPPET,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_SNIPPET,
                                   gParamSpecs[PROP_SNIPPET]);
}

static void
gb_source_snippet_completion_item_init (GbSourceSnippetCompletionItem *item)
{
  item->priv = gb_source_snippet_completion_item_get_instance_private (item);
}

static gchar *
get_label (GtkSourceCompletionProposal *p)
{
  GbSourceSnippetCompletionItem *item = GB_SOURCE_SNIPPET_COMPLETION_ITEM (p);
  const gchar *trigger = NULL;
  const gchar *description = NULL;

  if (item->priv->snippet)
    {
      trigger = gb_source_snippet_get_trigger (item->priv->snippet);
      description = gb_source_snippet_get_description (item->priv->snippet);
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
