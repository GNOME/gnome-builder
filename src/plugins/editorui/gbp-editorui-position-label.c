/* gbp-editorui-position-label.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-editorui-position-label"

#include "config.h"

#include <glib/gi18n.h>

#include "gbp-editorui-position-label.h"

struct _GbpEditoruiPositionLabel
{
  GtkWidget parent_instance;
  GtkLabel *label;
};

G_DEFINE_TYPE (GbpEditoruiPositionLabel, gbp_editorui_position_label, GTK_TYPE_WIDGET)

static void
gbp_editorui_position_label_dispose (GObject *object)
{
  GbpEditoruiPositionLabel *self = (GbpEditoruiPositionLabel *)object;

  gtk_widget_unparent (GTK_WIDGET (self->label));
  self->label = NULL;

  G_OBJECT_CLASS (gbp_editorui_position_label_parent_class)->dispose (object);
}

static void
gbp_editorui_position_label_class_init (GbpEditoruiPositionLabelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = gbp_editorui_position_label_dispose;

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/editorui/gbp-editorui-position-label.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpEditoruiPositionLabel, label);
}

static void
gbp_editorui_position_label_init (GbpEditoruiPositionLabel *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

void
gbp_editorui_position_label_update (GbpEditoruiPositionLabel *self,
                                    guint                     line,
                                    guint                     column,
                                    guint                     range)
{
  char str[64];

  g_return_if_fail (GBP_IS_EDITORUI_POSITION_LABEL (self));

  if (range == 0)
    /* translators: the first %u is replaced with the line number and the second with the column. */
    g_snprintf (str, sizeof str, _("Ln %u, Col %u"), line + 1, column + 1);
  else
    /* translators: the first %u is replaced with the line number, the second with the column and the third one with the number of selected characters. */
    g_snprintf (str, sizeof str, _("Ln %u, Col %u (Sel: %u)"), line + 1, column + 1, range);
  gtk_label_set_label (self->label, str);
}
