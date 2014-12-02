/* gb-command-vim.c
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

#include <glib/gi18n.h>

#include "gb-command-vim.h"
#include "gb-editor-frame-private.h"
#include "gb-editor-tab.h"
#include "gb-editor-tab-private.h"
#include "gb-source-vim.h"

struct _GbCommandVimPrivate
{
  GbEditorTab *tab;
  gchar       *command_text;
};

G_DEFINE_TYPE_WITH_PRIVATE (GbCommandVim, gb_command_vim, GB_TYPE_COMMAND)

enum {
  PROP_0,
  PROP_COMMAND_TEXT,
  PROP_TAB,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

GbEditorTab *
gb_command_vim_get_tab (GbCommandVim *vim)
{
  g_return_val_if_fail (GB_IS_COMMAND_VIM (vim), NULL);

  return vim->priv->tab;
}

static void
gb_command_vim_set_tab (GbCommandVim *vim,
                        GbEditorTab  *tab)
{
  g_return_if_fail (GB_IS_COMMAND_VIM (vim));
  g_return_if_fail (!tab || GB_IS_EDITOR_TAB (tab));

  if (tab != vim->priv->tab)
    {
      if (vim->priv->tab)
        {
          g_object_remove_weak_pointer (G_OBJECT (vim->priv->tab),
                                        (gpointer *)&vim->priv->tab);
          vim->priv->tab = NULL;
        }

      if (tab)
        {
          vim->priv->tab = tab;
          g_object_add_weak_pointer (G_OBJECT (vim->priv->tab),
                                     (gpointer *)&vim->priv->tab);
        }

      g_object_notify_by_pspec (G_OBJECT (vim), gParamSpecs [PROP_TAB]);
    }
}

const gchar *
gb_command_vim_get_command_text (GbCommandVim *vim)
{
  g_return_val_if_fail (GB_IS_COMMAND_VIM (vim), NULL);

  return vim->priv->command_text;
}

void
gb_command_vim_set_command_text (GbCommandVim *vim,
                                 const gchar  *command_text)
{
  g_return_if_fail (GB_IS_COMMAND_VIM (vim));
  g_return_if_fail (command_text);

  if (command_text != vim->priv->command_text)
    {
      g_free (vim->priv->command_text);
      vim->priv->command_text = g_strdup (command_text);
      g_object_notify_by_pspec (G_OBJECT (vim),
                                gParamSpecs [PROP_COMMAND_TEXT]);
    }
}

static GbCommandResult *
gb_command_vim_execute (GbCommand *command)
{
  GbCommandVim *self = (GbCommandVim *)command;

  g_return_val_if_fail (GB_IS_COMMAND_VIM (self), NULL);

  if (self->priv->tab && self->priv->command_text)
    {
      GbSourceVim *vim;
      GbEditorFrame *frame;

      frame = gb_editor_tab_get_last_frame (self->priv->tab);
      vim = gb_source_view_get_vim (frame->priv->source_view);
      gb_source_vim_execute_command (vim, self->priv->command_text);
    }

  return NULL;
}

static void
gb_command_vim_finalize (GObject *object)
{
  GbCommandVimPrivate *priv = GB_COMMAND_VIM (object)->priv;

  gb_command_vim_set_tab (GB_COMMAND_VIM (object), NULL);
  g_clear_pointer (&priv->command_text, g_free);

  G_OBJECT_CLASS (gb_command_vim_parent_class)->finalize (object);
}

static void
gb_command_vim_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  GbCommandVim *self = GB_COMMAND_VIM (object);

  switch (prop_id)
    {
    case PROP_COMMAND_TEXT:
      g_value_set_string (value, gb_command_vim_get_command_text (self));
      break;

    case PROP_TAB:
      g_value_set_object (value, gb_command_vim_get_tab (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_command_vim_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  GbCommandVim *self = GB_COMMAND_VIM (object);

  switch (prop_id)
    {
    case PROP_COMMAND_TEXT:
      gb_command_vim_set_command_text (self, g_value_get_string (value));
      break;

    case PROP_TAB:
      gb_command_vim_set_tab (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_command_vim_class_init (GbCommandVimClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GbCommandClass *command_class = GB_COMMAND_CLASS (klass);

  object_class->finalize = gb_command_vim_finalize;
  object_class->get_property = gb_command_vim_get_property;
  object_class->set_property = gb_command_vim_set_property;

  command_class->execute = gb_command_vim_execute;

  gParamSpecs [PROP_COMMAND_TEXT] =
    g_param_spec_string ("command-text",
                         _("Command Text"),
                         _("The command text to execute"),
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_COMMAND_TEXT,
                                   gParamSpecs [PROP_COMMAND_TEXT]);

  gParamSpecs [PROP_TAB] =
    g_param_spec_object ("tab",
                         _("Tab"),
                         _("The editor tab to modify."),
                         GB_TYPE_EDITOR_TAB,
                         (G_PARAM_READWRITE |
                          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_TAB,
                                   gParamSpecs [PROP_TAB]);
}

static void
gb_command_vim_init (GbCommandVim *self)
{
  self->priv = gb_command_vim_get_instance_private (self);
}
