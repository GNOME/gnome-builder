/* gb-command-vim-provider.c
 *
 * Copyright (C) 2014 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "vim-command-provider"

#if 0

#include "gb-editor-view.h"
#include "gb-editor-frame-private.h"
#include "gb-command-vim.h"
#include "gb-command-vim-provider.h"
#include "gb-source-view.h"
#include "gb-source-vim.h"
#include "gb-workbench.h"
#include "trie.h"

struct _GbCommandVimProviderPrivate
{
  Trie *trie;
};

G_DEFINE_TYPE_WITH_PRIVATE (GbCommandVimProvider, gb_command_vim_provider,
                            GB_TYPE_COMMAND_PROVIDER)

GbCommandProvider *
gb_command_vim_provider_new (GbWorkbench *workbench)
{
  return g_object_new (GB_TYPE_COMMAND_VIM_PROVIDER,
                       "workbench", workbench,
                       NULL);
}

static GbCommand *
gb_command_vim_provider_lookup (GbCommandProvider *provider,
                                const gchar       *command_text)
{
  GbWorkbench *workbench;
  GSettings *settings;
  GbDocumentView *active_view;
  GbEditorFrame *frame;

  g_return_val_if_fail (GB_IS_COMMAND_VIM_PROVIDER (provider), NULL);
  g_return_val_if_fail (command_text, NULL);

  /* Fetch our editor gsettings */
  settings = g_object_get_data (G_OBJECT (provider), "editor-settings");
  if (!G_IS_SETTINGS (settings))
    return NULL;

  /* Make sure vim-mode is enabled */
  if (!g_settings_get_boolean (settings, "vim-mode"))
    return NULL;

  /* Make sure we have a workbench */
  workbench = gb_command_provider_get_workbench (provider);
  if (!GB_IS_WORKBENCH (workbench))
    return NULL;

  /* Make sure we have an editor tab last focused */
  active_view = gb_command_provider_get_active_view (provider);
  if (!GB_IS_EDITOR_VIEW (active_view))
    return NULL;

  /* TODO: Perhaps get the last focused frame? */
  frame = gb_editor_view_get_frame1 (GB_EDITOR_VIEW (active_view));
  if (!GB_IS_EDITOR_FRAME (frame))
    return NULL;

  /* See if GbEditorVim recognizes this command */
  if (gb_source_vim_is_command (command_text))
    return g_object_new (GB_TYPE_COMMAND_VIM,
                         "command-text", command_text,
                         "source-view", frame->priv->source_view,
                         NULL);

  return NULL;
}

static gboolean
traverse_func (Trie        *trie,
               const gchar *key,
               gpointer     value,
               gpointer     user_data)
{
  GPtrArray *ar = user_data;
  g_ptr_array_add (ar, g_strdup (key));
  return FALSE;
}

static void
gb_command_vim_provider_complete (GbCommandProvider *provider,
                                  GPtrArray         *completions,
                                  const gchar       *initial_command_text)
{
  GbCommandVimProvider *self = (GbCommandVimProvider *)provider;

  g_return_if_fail (GB_IS_COMMAND_VIM_PROVIDER (provider));
  g_return_if_fail (completions);
  g_return_if_fail (initial_command_text);

  trie_traverse (self->priv->trie,
                 initial_command_text,
                 G_PRE_ORDER,
                 G_TRAVERSE_LEAVES,
                 -1,
                 traverse_func,
                 completions);
}

static void
gb_command_vim_provider_finalize (GObject *object)
{
  GbCommandVimProvider *provider = (GbCommandVimProvider *)object;

  trie_destroy (provider->priv->trie);
  provider->priv->trie = NULL;

  G_OBJECT_CLASS (gb_command_vim_provider_parent_class)->finalize (object);
}

static void
gb_command_vim_provider_class_init (GbCommandVimProviderClass *klass)
{
  GbCommandProviderClass *provider_class = GB_COMMAND_PROVIDER_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gb_command_vim_provider_finalize;

  provider_class->lookup = gb_command_vim_provider_lookup;
  provider_class->complete = gb_command_vim_provider_complete;
}

static void
gb_command_vim_provider_init (GbCommandVimProvider *self)
{
  static const gchar *commands[] = {
    "colorscheme ",
    "edit",
    "nohl",
    "set ",
    "sort",
    "split",
    "syntax ",
    "vsplit",
    NULL
  };
  GSettings *settings;
  guint i;

  self->priv = gb_command_vim_provider_get_instance_private (self);

  self->priv->trie = trie_new (NULL);
  for (i = 0; commands [i]; i++)
    trie_insert (self->priv->trie, commands [i], (gchar *)commands [i]);

  settings = g_settings_new ("org.gnome.builder.editor");
  g_object_set_data_full (G_OBJECT (self), "editor-settings", settings,
                          g_object_unref);
}

#endif
