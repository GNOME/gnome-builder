/* gbp-command-bar-suggestion.c
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "gbp-command-bar-suggestion"

#include "config.h"

#include "gbp-command-bar-suggestion.h"

struct _GbpCommandBarSuggestion
{
  DzlSuggestion  parent_instance;
  IdeCommand    *command;
} GbpCommandBarSuggestionPrivate;

enum {
  PROP_0,
  PROP_COMMAND,
  N_PROPS
};

G_DEFINE_TYPE (GbpCommandBarSuggestion, gbp_command_bar_suggestion, DZL_TYPE_SUGGESTION)

static GParamSpec *properties [N_PROPS];

static void
gbp_command_bar_suggestion_set_command (GbpCommandBarSuggestion *self,
                                        IdeCommand              *command)
{
  g_return_if_fail (GBP_IS_COMMAND_BAR_SUGGESTION (self));
  g_return_if_fail (IDE_IS_COMMAND (command));

  if (g_set_object (&self->command, command))
    {
      g_autofree gchar *title = ide_command_get_title (command);
      g_autofree gchar *subtitle = ide_command_get_subtitle (command);

      dzl_suggestion_set_title (DZL_SUGGESTION (self), title);
      dzl_suggestion_set_subtitle (DZL_SUGGESTION (self), subtitle);
    }
}

static GIcon *
gbp_command_bar_suggestion_get_icon (DzlSuggestion *suggestion)
{
  GbpCommandBarSuggestion *self = (GbpCommandBarSuggestion *)suggestion;
  IdeCommand *command;

  g_assert (GBP_IS_COMMAND_BAR_SUGGESTION (self));

  if ((command = gbp_command_bar_suggestion_get_command (self)))
    return ide_command_get_icon (command);

  return NULL;
}

static void
gbp_command_bar_suggestion_dispose (GObject *object)
{
  GbpCommandBarSuggestion *self = (GbpCommandBarSuggestion *)object;

  g_clear_object (&self->command);

  G_OBJECT_CLASS (gbp_command_bar_suggestion_parent_class)->dispose (object);
}

static void
gbp_command_bar_suggestion_get_property (GObject    *object,
                                         guint       prop_id,
                                         GValue     *value,
                                         GParamSpec *pspec)
{
  GbpCommandBarSuggestion *self = GBP_COMMAND_BAR_SUGGESTION (object);

  switch (prop_id)
    {
    case PROP_COMMAND:
      g_value_set_object (value, gbp_command_bar_suggestion_get_command (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_command_bar_suggestion_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  GbpCommandBarSuggestion *self = GBP_COMMAND_BAR_SUGGESTION (object);

  switch (prop_id)
    {
    case PROP_COMMAND:
      gbp_command_bar_suggestion_set_command (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_command_bar_suggestion_class_init (GbpCommandBarSuggestionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  DzlSuggestionClass *suggestion_class = DZL_SUGGESTION_CLASS (klass);

  object_class->dispose = gbp_command_bar_suggestion_dispose;
  object_class->get_property = gbp_command_bar_suggestion_get_property;
  object_class->set_property = gbp_command_bar_suggestion_set_property;

  suggestion_class->get_icon = gbp_command_bar_suggestion_get_icon;

  properties [PROP_COMMAND] =
    g_param_spec_object ("command",
                         "Command",
                         "The command for the suggestion",
                         IDE_TYPE_COMMAND,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_command_bar_suggestion_init (GbpCommandBarSuggestion *self)
{
}

/**
 * gbp_command_bar_suggestion_get_command:
 * @self: a #GbpCommandBarSuggestion
 *
 * Returns: (transfer none): an #IdeCommand
 *
 * Since: 3.32
 */
IdeCommand *
gbp_command_bar_suggestion_get_command (GbpCommandBarSuggestion *self)
{
  g_return_val_if_fail (GBP_IS_COMMAND_BAR_SUGGESTION (self), NULL);

  return self->command;
}

GbpCommandBarSuggestion *
gbp_command_bar_suggestion_new (IdeCommand *command)
{
  g_return_val_if_fail (IDE_IS_COMMAND (command), NULL);

  return g_object_new (GBP_TYPE_COMMAND_BAR_SUGGESTION,
                       "command", command,
                       NULL);
}
