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

#define G_LOG_DOMAIN "gb-command-vim"

#include <glib/gi18n.h>
#include <ide.h>

#include "gb-command-vim.h"
#include "gb-vim.h"

struct _GbCommandVim
{
  GbCommand      parent_instance;

  IdeSourceView *source_view;
  gchar         *command_text;
};

G_DEFINE_TYPE (GbCommandVim, gb_command_vim, GB_TYPE_COMMAND)

enum {
  PROP_0,
  PROP_COMMAND_TEXT,
  PROP_SOURCE_VIEW,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

IdeSourceView *
gb_command_vim_get_source_view (GbCommandVim *vim)
{
  g_return_val_if_fail (GB_IS_COMMAND_VIM (vim), NULL);

  return vim->source_view;
}

static void
gb_command_vim_set_source_view (GbCommandVim  *vim,
                                IdeSourceView *source_view)
{
  g_return_if_fail (GB_IS_COMMAND_VIM (vim));
  g_return_if_fail (IDE_IS_SOURCE_VIEW (source_view));

  if (ide_set_weak_pointer (&vim->source_view, source_view))
    g_object_notify_by_pspec (G_OBJECT (vim), gParamSpecs [PROP_SOURCE_VIEW]);
}

const gchar *
gb_command_vim_get_command_text (GbCommandVim *vim)
{
  g_return_val_if_fail (GB_IS_COMMAND_VIM (vim), NULL);

  return vim->command_text;
}

void
gb_command_vim_set_command_text (GbCommandVim *vim,
                                 const gchar  *command_text)
{
  g_return_if_fail (GB_IS_COMMAND_VIM (vim));
  g_return_if_fail (command_text);

  if (command_text != vim->command_text)
    {
      g_free (vim->command_text);
      vim->command_text = g_strdup (command_text);
      g_object_notify_by_pspec (G_OBJECT (vim), gParamSpecs [PROP_COMMAND_TEXT]);
    }
}

static GbCommandResult *
gb_command_vim_execute (GbCommand *command)
{
  GbCommandVim *self = (GbCommandVim *)command;

  g_return_val_if_fail (GB_IS_COMMAND_VIM (self), NULL);

  if (self->source_view)
    {
      GtkSourceView *source_view = (GtkSourceView *)self->source_view;
      GError *error = NULL;

      IDE_TRACE_MSG ("Executing Vim command: %s", self->command_text);

      if (!gb_vim_execute (source_view, self->command_text, &error))
        {
          g_warning ("%s", error->message);
          g_clear_error (&error);
        }
    }

  return NULL;
}

static void
gb_command_vim_finalize (GObject *object)
{
  GbCommandVim *self = GB_COMMAND_VIM (object);

  ide_clear_weak_pointer (&self->source_view);
  g_clear_pointer (&self->command_text, g_free);

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

  gParamSpecs [PROP_SOURCE_VIEW] =
    g_param_spec_object ("source-view",
                         _("Source View"),
                         _("The source view to modify."),
                         IDE_TYPE_SOURCE_VIEW,
                         (G_PARAM_READWRITE |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, gParamSpecs);
}

static void
gb_command_vim_init (GbCommandVim *self)
{
}
