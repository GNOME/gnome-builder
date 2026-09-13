/* gbp-acp-message-row.c
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

#define G_LOG_DOMAIN "gbp-acp-message-row"

#include "config.h"

#include <glib/gi18n.h>

#include "gbp-acp-message-row.h"

struct _GbpAcpMessageRow
{
  GtkBox    parent_instance;
  gchar    *role;
  GString  *text;
  GtkWidget *body;
};

G_DEFINE_FINAL_TYPE (GbpAcpMessageRow, gbp_acp_message_row, GTK_TYPE_BOX)

static void
gbp_acp_message_row_finalize (GObject *object)
{
  GbpAcpMessageRow *self = (GbpAcpMessageRow *)object;

  g_clear_pointer (&self->role, g_free);
  if (self->text != NULL)
    {
      g_string_free (self->text, TRUE);
      self->text = NULL;
    }

  G_OBJECT_CLASS (gbp_acp_message_row_parent_class)->finalize (object);
}

static void
gbp_acp_message_row_class_init (GbpAcpMessageRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gbp_acp_message_row_finalize;
}

static void
gbp_acp_message_row_init (GbpAcpMessageRow *self)
{
  gtk_orientable_set_orientation (GTK_ORIENTABLE (self), GTK_ORIENTATION_VERTICAL);
  gtk_box_set_spacing (GTK_BOX (self), 4);
  gtk_widget_set_margin_start (GTK_WIDGET (self), 6);
  gtk_widget_set_margin_end (GTK_WIDGET (self), 6);
  gtk_widget_set_margin_top (GTK_WIDGET (self), 4);
  gtk_widget_set_margin_bottom (GTK_WIDGET (self), 4);
}

GtkWidget *
gbp_acp_message_row_new (const gchar *role)
{
  GbpAcpMessageRow *self;
  GtkWidget *title;
  const gchar *title_text;

  g_return_val_if_fail (role != NULL, NULL);

  self = g_object_new (GBP_TYPE_ACP_MESSAGE_ROW, NULL);
  self->role = g_strdup (role);
  self->text = g_string_new (NULL);

  if (g_strcmp0 (role, "user") == 0)
    {
      title_text = _("You");
      gtk_widget_add_css_class (GTK_WIDGET (self), "acp-user");
    }
  else if (g_strcmp0 (role, "thought") == 0)
    {
      title_text = _("Thinking");
      gtk_widget_add_css_class (GTK_WIDGET (self), "acp-thought");
    }
  else
    {
      title_text = _("Agent");
      gtk_widget_add_css_class (GTK_WIDGET (self), "acp-agent");
    }

  title = gtk_label_new (title_text);
  gtk_widget_set_halign (title, GTK_ALIGN_START);
  gtk_widget_add_css_class (title, "caption");
  gtk_widget_add_css_class (title, "dim-label");

  self->body = gtk_label_new (NULL);
  gtk_label_set_wrap (GTK_LABEL (self->body), TRUE);
  gtk_label_set_selectable (GTK_LABEL (self->body), TRUE);
  gtk_label_set_xalign (GTK_LABEL (self->body), 0.0f);
  gtk_widget_set_margin_start (self->body, 12);
  gtk_widget_set_margin_end (self->body, 12);
  gtk_widget_set_margin_top (self->body, 8);
  gtk_widget_set_margin_bottom (self->body, 8);

  if (g_strcmp0 (role, "thought") == 0)
    {
      GtkWidget *expander = gtk_expander_new (NULL);
      gtk_expander_set_label_widget (GTK_EXPANDER (expander), title);
      gtk_expander_set_child (GTK_EXPANDER (expander), self->body);
      gtk_expander_set_expanded (GTK_EXPANDER (expander), FALSE);
      gtk_box_append (GTK_BOX (self), expander);
    }
  else
    {
      gtk_box_append (GTK_BOX (self), title);
      gtk_box_append (GTK_BOX (self), self->body);
    }

  return GTK_WIDGET (self);
}

void
gbp_acp_message_row_append_text (GbpAcpMessageRow *self,
                                 const gchar      *text)
{
  g_return_if_fail (GBP_IS_ACP_MESSAGE_ROW (self));
  g_return_if_fail (text != NULL);

  g_string_append (self->text, text);
  gtk_label_set_text (GTK_LABEL (self->body), self->text->str);
}

void
gbp_acp_message_row_set_complete (GbpAcpMessageRow *self,
                                  gboolean          complete)
{
  g_return_if_fail (GBP_IS_ACP_MESSAGE_ROW (self));

  if (complete)
    gtk_widget_remove_css_class (GTK_WIDGET (self), "acp-streaming");
  else
    gtk_widget_add_css_class (GTK_WIDGET (self), "acp-streaming");
}

const gchar *
gbp_acp_message_row_get_role (GbpAcpMessageRow *self)
{
  g_return_val_if_fail (GBP_IS_ACP_MESSAGE_ROW (self), NULL);
  return self->role;
}

const gchar *
gbp_acp_message_row_get_text (GbpAcpMessageRow *self)
{
  g_return_val_if_fail (GBP_IS_ACP_MESSAGE_ROW (self), NULL);
  return self->text->str;
}
