/* ide-ctags-completion-item.c
 *
 * Copyright 2015 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "ide-ctags-completion-item"

#include <dazzle.h>
#include <glib/gi18n.h>

#include "ide-ctags-completion-item.h"
#include "ide-ctags-completion-provider.h"
#include "ide-ctags-completion-provider-private.h"
#include "ide-ctags-index.h"

struct _IdeCtagsCompletionItem
{
  IdeCompletionItem           parent_instance;
  const IdeCtagsIndexEntry   *entry;
  IdeCtagsCompletionProvider *provider;
};

static void proposal_iface_init (GtkSourceCompletionProposalIface *iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (IdeCtagsCompletionItem,
                                ide_ctags_completion_item,
                                IDE_TYPE_COMPLETION_ITEM,
                                0,
                                G_IMPLEMENT_INTERFACE (GTK_SOURCE_TYPE_COMPLETION_PROPOSAL, proposal_iface_init))

DZL_DEFINE_COUNTER (instances, "IdeCtagsCompletionItem", "Instances", "Number of IdeCtagsCompletionItems")

IdeCtagsCompletionItem *
ide_ctags_completion_item_new (IdeCtagsCompletionProvider *provider,
                               const IdeCtagsIndexEntry   *entry)
{
  IdeCtagsCompletionItem *self;

  g_return_val_if_fail (entry != NULL, NULL);

  self = g_object_new (IDE_TYPE_CTAGS_COMPLETION_ITEM, NULL);
  self->provider = provider;
  self->entry = entry;

  return self;
}

gint
ide_ctags_completion_item_compare (IdeCtagsCompletionItem *itema,
                                   IdeCtagsCompletionItem *itemb)
{
  return ide_ctags_index_entry_compare (itema->entry, itemb->entry);
}

static gboolean
ide_ctags_completion_item_match (IdeCompletionItem *item,
                                 const gchar       *query,
                                 const gchar       *casefold)
{
  IdeCtagsCompletionItem *self = (IdeCtagsCompletionItem *)item;

  if (ide_completion_item_fuzzy_match (self->entry->name, casefold, &item->priority))
    {
      if (!dzl_str_equal0 (self->entry->name, query))
        return TRUE;
    }

  return FALSE;
}

static void
ide_ctags_completion_item_finalize (GObject *object)
{
  G_OBJECT_CLASS (ide_ctags_completion_item_parent_class)->finalize (object);

  DZL_COUNTER_DEC (instances);
}

static void
ide_ctags_completion_item_class_init (IdeCtagsCompletionItemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeCompletionItemClass *item_class = IDE_COMPLETION_ITEM_CLASS (klass);

  object_class->finalize = ide_ctags_completion_item_finalize;

  item_class->match = ide_ctags_completion_item_match;
}

static void
ide_ctags_completion_item_class_finalize (IdeCtagsCompletionItemClass *klass)
{
}

static void
ide_ctags_completion_item_init (IdeCtagsCompletionItem *self)
{
  DZL_COUNTER_INC (instances);
}

static gchar *
get_markup (GtkSourceCompletionProposal *proposal)
{
  IdeCtagsCompletionItem *self = (IdeCtagsCompletionItem *)proposal;

  if (self->provider->current_word != NULL)
    return ide_completion_item_fuzzy_highlight (self->entry->name, self->provider->current_word);

  return g_strdup (self->entry->name);
}

static gchar *
get_text (GtkSourceCompletionProposal *proposal)
{
  IdeCtagsCompletionItem *self = (IdeCtagsCompletionItem *)proposal;

  return g_strdup (self->entry->name);
}

static const gchar *
get_icon_name (GtkSourceCompletionProposal *proposal)
{
  IdeCtagsCompletionItem *self = (IdeCtagsCompletionItem *)proposal;
  const gchar *icon_name = NULL;

  if (self->entry == NULL)
    return NULL;

  switch (self->entry->kind)
    {
    case IDE_CTAGS_INDEX_ENTRY_CLASS_NAME:
      icon_name = "lang-class-symbolic";
      break;

    case IDE_CTAGS_INDEX_ENTRY_ENUMERATOR:
      icon_name = "lang-enum-value-symbolic";
      break;

    case IDE_CTAGS_INDEX_ENTRY_ENUMERATION_NAME:
      icon_name = "lang-enum-symbolic";
      break;

    case IDE_CTAGS_INDEX_ENTRY_PROTOTYPE:
    case IDE_CTAGS_INDEX_ENTRY_FUNCTION:
      icon_name = "lang-function-symbolic";
      break;

    case IDE_CTAGS_INDEX_ENTRY_FILE_NAME:
      icon_name = "text-x-generic-symbolic";
      break;

    case IDE_CTAGS_INDEX_ENTRY_IMPORT:
      icon_name = "lang-include-symbolic";
      break;

    case IDE_CTAGS_INDEX_ENTRY_MEMBER:
      icon_name = "struct-field-symbolic";
      break;

    case IDE_CTAGS_INDEX_ENTRY_UNION:
      icon_name = "lang-union-symbolic";
      break;

    case IDE_CTAGS_INDEX_ENTRY_TYPEDEF:
      icon_name = "lang-typedef-symbolic";
      break;

    case IDE_CTAGS_INDEX_ENTRY_STRUCTURE:
      icon_name = "lang-struct-symbolic";
      break;

    case IDE_CTAGS_INDEX_ENTRY_VARIABLE:
      icon_name = "lang-variable-symbolic";
      break;

    case IDE_CTAGS_INDEX_ENTRY_DEFINE:
      icon_name = "lang-define-symbolic";
      break;

    case IDE_CTAGS_INDEX_ENTRY_ANCHOR:
    default:
      break;
    }

  return icon_name;
}

static void
proposal_iface_init (GtkSourceCompletionProposalIface *iface)
{
  iface->get_markup = get_markup;
  iface->get_text = get_text;
  iface->get_icon_name = get_icon_name;
}

void
_ide_ctags_completion_item_register_type (GTypeModule *module)
{
  ide_ctags_completion_item_register_type (module);
}

gboolean
ide_ctags_completion_item_is_function (IdeCtagsCompletionItem *self)
{
  g_return_val_if_fail (IDE_IS_CTAGS_COMPLETION_ITEM (self), FALSE);
  g_return_val_if_fail (self->entry != NULL, FALSE);

  return self->entry->kind == IDE_CTAGS_INDEX_ENTRY_FUNCTION;
}

IdeSourceSnippet *
ide_ctags_completion_item_get_snippet (IdeCtagsCompletionItem *self,
                                       IdeFileSettings        *file_settings)
{
  g_autoptr(IdeSourceSnippet) ret = NULL;
  g_autoptr(IdeSourceSnippetChunk) chunk1 = NULL;

  g_return_val_if_fail (IDE_IS_CTAGS_COMPLETION_ITEM (self), NULL);
  g_return_val_if_fail (!file_settings || IDE_IS_FILE_SETTINGS (file_settings), NULL);

  ret = ide_source_snippet_new (NULL, NULL);

  chunk1 = ide_source_snippet_chunk_new ();
  ide_source_snippet_chunk_set_spec (chunk1, self->entry->name);
  ide_source_snippet_add_chunk (ret, chunk1);

  if (ide_ctags_completion_item_is_function (self))
    {
      g_autoptr(IdeSourceSnippetChunk) chunk2 = ide_source_snippet_chunk_new ();
      g_autoptr(IdeSourceSnippetChunk) chunk3 = ide_source_snippet_chunk_new ();
      g_autoptr(IdeSourceSnippetChunk) chunk4 = ide_source_snippet_chunk_new ();
      IdeSpacesStyle style = IDE_SPACES_STYLE_BEFORE_LEFT_PAREN;

      if (file_settings != NULL)
        style = ide_file_settings_get_spaces_style (file_settings);

      if (style & IDE_SPACES_STYLE_BEFORE_LEFT_PAREN)
        ide_source_snippet_chunk_set_spec (chunk2, " (");
      else
        ide_source_snippet_chunk_set_spec (chunk2, "(");

      ide_source_snippet_chunk_set_tab_stop (chunk3, 0);
      ide_source_snippet_chunk_set_spec (chunk4, ")");

      ide_source_snippet_add_chunk (ret, chunk2);
      ide_source_snippet_add_chunk (ret, chunk3);
      ide_source_snippet_add_chunk (ret, chunk4);
    }

  return g_steal_pointer (&ret);
}
