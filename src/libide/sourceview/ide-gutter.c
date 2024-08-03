/* ide-gutter.c
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

#define G_LOG_DOMAIN "ide-gutter"

#include "config.h"

#include "ide-gutter.h"

G_DEFINE_INTERFACE (IdeGutter, ide_gutter, GTK_SOURCE_TYPE_GUTTER_RENDERER)

enum {
  STYLE_CHANGED,
  N_SIGNALS
};

static guint signals [N_SIGNALS];

static void
ide_gutter_default_init (IdeGutterInterface *iface)
{
  g_object_interface_install_property (iface,
                                       g_param_spec_boolean ("show-line-changes",
                                                             "Show Line Changes",
                                                             "If line changes should be displayed",
                                                             FALSE,
                                                             (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_interface_install_property (iface,
                                       g_param_spec_boolean ("show-line-diagnostics",
                                                             "Show Line Diagnostics",
                                                             "If line diagnostics should be displayed",
                                                             FALSE,
                                                             (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_interface_install_property (iface,
                                       g_param_spec_boolean ("show-line-numbers",
                                                             "Show Line Numbers",
                                                             "If line numbers should be displayed",
                                                             FALSE,
                                                             (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_interface_install_property (iface,
                                       g_param_spec_boolean ("show-relative-line-numbers",
                                                             "Show Relative Line Numbers",
                                                             "If line numbers should be displayed relative to the cursor line",
                                                             FALSE,
                                                             (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_interface_install_property (iface,
                                       g_param_spec_boolean ("show-line-selection-styling",
                                                             "Show Line Selection Styling",
                                                             "If selection styling should be used for line numbers",
                                                             FALSE,
                                                             (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  signals [STYLE_CHANGED] =
    g_signal_new ("style-changed",
                  G_TYPE_FROM_INTERFACE (iface),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdeGutterInterface, style_changed),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);
}

void
ide_gutter_style_changed (IdeGutter *self)
{
  g_return_if_fail (IDE_IS_GUTTER (self));

  g_signal_emit (self, signals [STYLE_CHANGED], 0);
}

gboolean
ide_gutter_get_show_line_changes (IdeGutter *self)
{
  gboolean ret;
  g_object_get (self, "show-line-changes", &ret, NULL);
  return ret;
}

gboolean
ide_gutter_get_show_line_numbers (IdeGutter *self)
{
  gboolean ret;
  g_object_get (self, "show-line-numbers", &ret, NULL);
  return ret;
}

gboolean
ide_gutter_get_show_relative_line_numbers (IdeGutter *self)
{
  gboolean ret;
  g_object_get (self, "show-relative-line-numbers", &ret, NULL);
  return ret;
}

gboolean
ide_gutter_get_show_line_diagnostics (IdeGutter *self)
{
  gboolean ret;
  g_object_get (self, "show-line-diagnostics", &ret, NULL);
  return ret;
}

gboolean
ide_gutter_get_show_line_selection_styling (IdeGutter *self)
{
  gboolean ret;
  g_object_get (self, "show-line-selection-styling", &ret, NULL);
  return ret;
}

void
ide_gutter_set_show_line_changes (IdeGutter *self,
                                  gboolean   show_line_changes)
{
  g_return_if_fail (IDE_IS_GUTTER (self));

  g_object_set (self, "show-line-changes", show_line_changes, NULL);
}

void
ide_gutter_set_show_line_numbers (IdeGutter *self,
                                  gboolean   show_line_numbers)
{
  g_return_if_fail (IDE_IS_GUTTER (self));

  g_object_set (self, "show-line-numbers", show_line_numbers, NULL);
}

void
ide_gutter_set_show_relative_line_numbers (IdeGutter *self,
                                           gboolean   show_relative_line_numbers)
{
  g_return_if_fail (IDE_IS_GUTTER (self));

  g_object_set (self, "show-relative-line-numbers", show_relative_line_numbers, NULL);
}

void
ide_gutter_set_show_line_diagnostics (IdeGutter *self,
                                      gboolean   show_line_diagnostics)
{
  g_return_if_fail (IDE_IS_GUTTER (self));

  g_object_set (self, "show-line-diagnostics", show_line_diagnostics, NULL);
}

void
ide_gutter_set_show_line_selection_styling (IdeGutter *self,
                                            gboolean show_line_selection_styling)
{
  g_return_if_fail (IDE_IS_GUTTER (self));

  g_object_set (self, "show-line-selection-styling", show_line_selection_styling, NULL);
}
