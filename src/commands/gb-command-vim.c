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
#include "gb-source-view.h"

struct _GbCommandVimPrivate
{
  GbSourceView *source_view;
  gchar        *command_text;
};

G_DEFINE_TYPE_WITH_PRIVATE (GbCommandVim, gb_command_vim, GB_TYPE_COMMAND)

enum {
  PROP_0,
  PROP_COMMAND_TEXT,
  PROP_SOURCE_VIEW,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

GbSourceView *
gb_command_vim_get_source_view (GbCommandVim *vim)
{
  g_return_val_if_fail (GB_IS_COMMAND_VIM (vim), NULL);

  return vim->priv->source_view;
}

static void
gb_command_vim_set_source_view (GbCommandVim *vim,
                                GbSourceView *source_view)
{
  g_return_if_fail (GB_IS_COMMAND_VIM (vim));
  g_return_if_fail (!source_view || GB_IS_SOURCE_VIEW (source_view));

  if (source_view != vim->priv->source_view)
    {
      if (vim->priv->source_view)
        {
          g_object_remove_weak_pointer (G_OBJECT (vim->priv->source_view),
                                        (gpointer *)&vim->priv->source_view);
          vim->priv->source_view = NULL;
        }

      if (source_view)
        {
          vim->priv->source_view = source_view;
          g_object_add_weak_pointer (G_OBJECT (vim->priv->source_view),
                                     (gpointer *)&vim->priv->source_view);
        }

      g_object_notify_by_pspec (G_OBJECT (vim), gParamSpecs [PROP_SOURCE_VIEW]);
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

  if (self->priv->source_view)
    {
      GbSourceVim *vim;

      vim = gb_source_view_get_vim (self->priv->source_view);
      gb_source_vim_execute_command (vim, self->priv->command_text);
    }

  return NULL;
}

static void
gb_command_vim_finalize (GObject *object)
{
  GbCommandVimPrivate *priv = GB_COMMAND_VIM (object)->priv;

  gb_command_vim_set_source_view (GB_COMMAND_VIM (object), NULL);
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

    case PROP_SOURCE_VIEW:
      g_value_set_object (value, gb_command_vim_get_source_view (self));
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

    case PROP_SOURCE_VIEW:
      gb_command_vim_set_source_view (self, g_value_get_object (value));
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

  gParamSpecs [PROP_SOURCE_VIEW] =
    g_param_spec_object ("source-view",
                         _("Source View"),
                         _("The source view to modify."),
                         GB_TYPE_SOURCE_VIEW,
                         (G_PARAM_READWRITE |
                          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_SOURCE_VIEW,
                                   gParamSpecs [PROP_SOURCE_VIEW]);
}

static void
gb_command_vim_init (GbCommandVim *self)
{
  self->priv = gb_command_vim_get_instance_private (self);
}
