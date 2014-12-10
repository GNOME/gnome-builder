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

#include "gb-editor-view.h"
#include "gb-editor-frame-private.h"
#include "gb-command-vim.h"
#include "gb-command-vim-provider.h"
#include "gb-source-view.h"
#include "gb-source-vim.h"

G_DEFINE_TYPE (GbCommandVimProvider, gb_command_vim_provider,
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

static void
gb_command_vim_provider_class_init (GbCommandVimProviderClass *klass)
{
  GbCommandProviderClass *provider_class = GB_COMMAND_PROVIDER_CLASS (klass);

  provider_class->lookup = gb_command_vim_provider_lookup;
}

static void
gb_command_vim_provider_init (GbCommandVimProvider *self)
{
  GSettings *settings;

  settings = g_settings_new ("org.gnome.builder.editor");
  g_object_set_data_full (G_OBJECT (self), "editor-settings", settings,
                          g_object_unref);
}
