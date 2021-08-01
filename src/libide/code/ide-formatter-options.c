/* ide-formatter-options.c
 *
 * Copyright 2017-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-formatter-options"

#include "config.h"

#include "ide-formatter-options.h"

struct _IdeFormatterOptions
{
  GObject parent_instance;
  guint   tab_width;
  guint   insert_spaces : 1;
};

enum {
  PROP_0,
  PROP_TAB_WIDTH,
  PROP_INSERT_SPACES,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (IdeFormatterOptions, ide_formatter_options, G_TYPE_OBJECT)

static GParamSpec *properties [N_PROPS];

static void
ide_formatter_options_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  IdeFormatterOptions *self = IDE_FORMATTER_OPTIONS (object);

  switch (prop_id)
    {
    case PROP_TAB_WIDTH:
      g_value_set_uint (value, ide_formatter_options_get_tab_width (self));
      break;

    case PROP_INSERT_SPACES:
      g_value_set_boolean (value, ide_formatter_options_get_insert_spaces (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_formatter_options_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  IdeFormatterOptions *self = IDE_FORMATTER_OPTIONS (object);

  switch (prop_id)
    {
    case PROP_TAB_WIDTH:
      ide_formatter_options_set_tab_width (self, g_value_get_uint (value));
      break;

    case PROP_INSERT_SPACES:
      ide_formatter_options_set_insert_spaces (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_formatter_options_class_init (IdeFormatterOptionsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = ide_formatter_options_get_property;
  object_class->set_property = ide_formatter_options_set_property;

  properties [PROP_INSERT_SPACES] =
    g_param_spec_boolean ("insert-spaces",
                          "Insert Spaces",
                          "Insert spaces instead of tabs",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_TAB_WIDTH] =
    g_param_spec_uint ("tab-width",
                       "Tab Width",
                       "The width of a tab in spaces",
                       1, 32, 8,
                       (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_formatter_options_init (IdeFormatterOptions *self)
{
  self->tab_width = 8;
}

guint
ide_formatter_options_get_tab_width (IdeFormatterOptions *self)
{
  g_return_val_if_fail (IDE_IS_FORMATTER_OPTIONS (self), 0);

  return self->tab_width;
}

void
ide_formatter_options_set_tab_width (IdeFormatterOptions *self,
                                     guint                tab_width)
{
  g_return_if_fail (IDE_IS_FORMATTER_OPTIONS (self));

  if (tab_width != self->tab_width)
    {
      self->tab_width = tab_width;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_TAB_WIDTH]);
    }
}

gboolean
ide_formatter_options_get_insert_spaces (IdeFormatterOptions *self)
{
  g_return_val_if_fail (IDE_IS_FORMATTER_OPTIONS (self), FALSE);

  return self->insert_spaces;
}

void
ide_formatter_options_set_insert_spaces (IdeFormatterOptions *self,
                                         gboolean             insert_spaces)
{
  g_return_if_fail (IDE_IS_FORMATTER_OPTIONS (self));

  insert_spaces = !!insert_spaces;

  if (insert_spaces != self->insert_spaces)
    {
      self->insert_spaces = insert_spaces;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_INSERT_SPACES]);
    }
}

IdeFormatterOptions *
ide_formatter_options_new (void)
{
  return g_object_new (IDE_TYPE_FORMATTER_OPTIONS, NULL);
}
