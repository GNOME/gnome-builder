/* gbp-acp-permission-row.c
 *
 * Copyright 2026 GNOME Builder contributors
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

#define G_LOG_DOMAIN "gbp-acp-permission-row"

#include "config.h"

#include <glib/gi18n.h>

#include "gbp-acp-permission-row.h"

struct _GbpAcpPermissionRow
{
  GtkBox    parent_instance;
  GtkWidget *buttons;
  gboolean   replied;
};

enum
{
  SIGNAL_REPLY,
  N_SIGNALS
};

static guint signals[N_SIGNALS];

G_DEFINE_FINAL_TYPE (GbpAcpPermissionRow, gbp_acp_permission_row, GTK_TYPE_BOX)

static void
gbp_acp_permission_row_class_init (GbpAcpPermissionRowClass *klass)
{
  signals[SIGNAL_REPLY] =
    g_signal_new ("reply",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 1, G_TYPE_STRING);
}

static void
gbp_acp_permission_row_init (GbpAcpPermissionRow *self)
{
  gtk_orientable_set_orientation (GTK_ORIENTABLE (self), GTK_ORIENTATION_VERTICAL);
  gtk_box_set_spacing (GTK_BOX (self), 8);
  gtk_widget_set_margin_start (GTK_WIDGET (self), 6);
  gtk_widget_set_margin_end (GTK_WIDGET (self), 6);
  gtk_widget_set_margin_top (GTK_WIDGET (self), 6);
  gtk_widget_set_margin_bottom (GTK_WIDGET (self), 6);
  gtk_widget_add_css_class (GTK_WIDGET (self), "acp-permission");
  gtk_widget_add_css_class (GTK_WIDGET (self), "card");
}

static void
on_button_clicked_cb (GtkButton           *button,
                      GbpAcpPermissionRow *self)
{
  const gchar *option_id;

  g_assert (GBP_IS_ACP_PERMISSION_ROW (self));

  if (self->replied)
    return;

  self->replied = TRUE;

  option_id = g_object_get_data (G_OBJECT (button), "option-id");
  g_signal_emit (self, signals[SIGNAL_REPLY], 0, option_id);

  gtk_widget_set_sensitive (self->buttons, FALSE);
}

static void
add_option (GbpAcpPermissionRow *self,
            GVariant            *option)
{
  const gchar *option_id = NULL;
  const gchar *name = NULL;
  const gchar *kind = NULL;
  GtkWidget *button;

  g_return_if_fail (option != NULL);
  g_return_if_fail (g_variant_is_of_type (option, G_VARIANT_TYPE_VARDICT));

  if (!g_variant_lookup (option, "optionId", "&s", &option_id))
    return;

  g_variant_lookup (option, "name", "&s", &name);
  g_variant_lookup (option, "kind", "&s", &kind);

  button = gtk_button_new_with_label (name ?: option_id);

  if (g_str_has_prefix (kind ?: "", "allow"))
    {
      gtk_widget_add_css_class (button, "suggested-action");
    }
  else if (g_str_has_prefix (kind ?: "", "reject"))
    {
      gtk_widget_add_css_class (button, "destructive-action");
    }

  g_object_set_data_full (G_OBJECT (button), "option-id", g_strdup (option_id), g_free);
  g_signal_connect (button, "clicked", G_CALLBACK (on_button_clicked_cb), self);

  gtk_box_append (GTK_BOX (self->buttons), button);
}

GtkWidget *
gbp_acp_permission_row_new (GVariant *tool_call,
                            GVariant *options)
{
  GbpAcpPermissionRow *self;
  GtkWidget *title;
  GtkWidget *subtitle;
  g_autoptr(GVariantIter) iter = NULL;
  const gchar *tool_title = NULL;
  GVariant *child;

  self = g_object_new (GBP_TYPE_ACP_PERMISSION_ROW, NULL);

  title = gtk_label_new (_("Permission required"));
  gtk_widget_set_halign (title, GTK_ALIGN_START);
  gtk_widget_add_css_class (title, "heading");
  gtk_box_append (GTK_BOX (self), title);

  if (tool_call != NULL && g_variant_lookup (tool_call, "title", "&s", &tool_title))
    {
      subtitle = gtk_label_new (tool_title);
      gtk_label_set_wrap (GTK_LABEL (subtitle), TRUE);
      gtk_label_set_xalign (GTK_LABEL (subtitle), 0.0f);
      gtk_widget_add_css_class (subtitle, "dim-label");
      gtk_box_append (GTK_BOX (self), subtitle);
    }

  self->buttons = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_widget_set_halign (self->buttons, GTK_ALIGN_END);
  gtk_box_append (GTK_BOX (self), self->buttons);

  if (options != NULL && g_variant_is_of_type (options, G_VARIANT_TYPE ("av")))
    {
      iter = g_variant_iter_new (options);
      while (g_variant_iter_loop (iter, "v", &child))
        add_option (self, child);
    }

  return GTK_WIDGET (self);
}
