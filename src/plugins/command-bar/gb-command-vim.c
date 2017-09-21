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

  GtkWidget     *active_widget;
  gchar         *command_text;
};

G_DEFINE_TYPE (GbCommandVim, gb_command_vim, GB_TYPE_COMMAND)

enum {
  PROP_0,
  PROP_COMMAND_TEXT,
  PROP_ACTIVE_WIDGET,
  LAST_PROP
};

static GParamSpec *properties [LAST_PROP];

GtkWidget *
gb_command_vim_get_active_widget (GbCommandVim *vim)
{
  g_return_val_if_fail (GB_IS_COMMAND_VIM (vim), NULL);

  return vim->active_widget;
}

static void
gb_command_vim_set_active_widget (GbCommandVim *vim,
                                  GtkWidget    *active_widget)
{
  g_return_if_fail (GB_IS_COMMAND_VIM (vim));
  g_return_if_fail (GTK_IS_WIDGET (active_widget));

  if (ide_set_weak_pointer (&vim->active_widget, active_widget))
    g_object_notify_by_pspec (G_OBJECT (vim), properties [PROP_ACTIVE_WIDGET]);
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
      g_object_notify_by_pspec (G_OBJECT (vim), properties [PROP_COMMAND_TEXT]);
    }
}

static GbCommandResult *
gb_command_vim_execute (GbCommand *command)
{
  GbCommandVim *self = (GbCommandVim *)command;

  g_return_val_if_fail (GB_IS_COMMAND_VIM (self), NULL);

  if (self->active_widget)
    {
      GError *error = NULL;

      IDE_TRACE_MSG ("Executing Vim command: %s", self->command_text);

      if (!gb_vim_execute (self->active_widget, self->command_text, &error))
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

  ide_clear_weak_pointer (&self->active_widget);
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

    case PROP_ACTIVE_WIDGET:
      g_value_set_object (value, gb_command_vim_get_active_widget (self));
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

    case PROP_ACTIVE_WIDGET:
      gb_command_vim_set_active_widget (self, g_value_get_object (value));
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

  properties [PROP_COMMAND_TEXT] =
    g_param_spec_string ("command-text",
                         "Command Text",
                         "The command text to execute",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_ACTIVE_WIDGET] =
    g_param_spec_object ("active-widget",
                         "Active widget",
                         "The active widget to act on.",
                         GTK_TYPE_WIDGET,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
gb_command_vim_init (GbCommandVim *self)
{
}
