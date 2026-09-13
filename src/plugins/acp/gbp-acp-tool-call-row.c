/* gbp-acp-tool-call-row.c
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

#define G_LOG_DOMAIN "gbp-acp-tool-call-row"

#include "config.h"

#include <glib/gi18n.h>

#include "gbp-acp-diff-view.h"
#include "gbp-acp-tool-call-row.h"

struct _GbpAcpToolCallRow
{
  GtkBox    parent_instance;
  gchar    *id;
  gchar    *kind;
  GtkWidget *expander;
  GtkWidget *icon;
  GtkWidget *title;
  GtkWidget *status;
  GtkWidget *spinner;
  GtkWidget *content_box;
  gboolean   has_content;
};

enum
{
  SIGNAL_OPEN_FILE,
  N_SIGNALS
};

static guint signals[N_SIGNALS];

G_DEFINE_FINAL_TYPE (GbpAcpToolCallRow, gbp_acp_tool_call_row, GTK_TYPE_BOX)

static void
gbp_acp_tool_call_row_finalize (GObject *object)
{
  GbpAcpToolCallRow *self = (GbpAcpToolCallRow *)object;

  g_clear_pointer (&self->id, g_free);
  g_clear_pointer (&self->kind, g_free);

  G_OBJECT_CLASS (gbp_acp_tool_call_row_parent_class)->finalize (object);
}

static void
gbp_acp_tool_call_row_class_init (GbpAcpToolCallRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gbp_acp_tool_call_row_finalize;

  signals[SIGNAL_OPEN_FILE] =
    g_signal_new ("open-file",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 1, G_TYPE_STRING);
}

static void
gbp_acp_tool_call_row_init (GbpAcpToolCallRow *self)
{
  gtk_orientable_set_orientation (GTK_ORIENTABLE (self), GTK_ORIENTATION_VERTICAL);
  gtk_widget_set_margin_start (GTK_WIDGET (self), 6);
  gtk_widget_set_margin_end (GTK_WIDGET (self), 6);
  gtk_widget_set_margin_top (GTK_WIDGET (self), 2);
  gtk_widget_set_margin_bottom (GTK_WIDGET (self), 2);
}

static const gchar *
icon_for_kind (const gchar *kind)
{
  if (g_strcmp0 (kind, "read") == 0)
    return "document-open-symbolic";
  if (g_strcmp0 (kind, "edit") == 0)
    return "document-edit-symbolic";
  if (g_strcmp0 (kind, "delete") == 0)
    return "edit-delete-symbolic";
  if (g_strcmp0 (kind, "move") == 0)
    return "go-next-symbolic";
  if (g_strcmp0 (kind, "search") == 0)
    return "edit-find-symbolic";
  if (g_strcmp0 (kind, "execute") == 0)
    return "utilities-terminal-symbolic";
  if (g_strcmp0 (kind, "think") == 0)
    return "emblem-important-symbolic";
  if (g_strcmp0 (kind, "fetch") == 0)
    return "web-browser-symbolic";

  return "applications-engineering-symbolic";
}

static const gchar *
status_text_for_status (const gchar *status)
{
  if (g_strcmp0 (status, "pending") == 0)
    return _("Pending");
  if (g_strcmp0 (status, "in_progress") == 0)
    return _("Running");
  if (g_strcmp0 (status, "completed") == 0)
    return _("Done");
  if (g_strcmp0 (status, "failed") == 0)
    return _("Failed");
  if (g_strcmp0 (status, "cancelled") == 0)
    return _("Cancelled");

  return status;
}

static void
gbp_acp_tool_call_row_set_status (GbpAcpToolCallRow *self,
                                  const gchar       *status)
{
  g_autofree gchar *css = NULL;

  g_return_if_fail (GBP_IS_ACP_TOOL_CALL_ROW (self));

  gtk_label_set_text (GTK_LABEL (self->status), status_text_for_status (status));

  gtk_widget_remove_css_class (self->status, "acp-status-pending");
  gtk_widget_remove_css_class (self->status, "acp-status-running");
  gtk_widget_remove_css_class (self->status, "acp-status-completed");
  gtk_widget_remove_css_class (self->status, "acp-status-failed");

  if (g_strcmp0 (status, "in_progress") == 0)
    {
      gtk_widget_add_css_class (self->status, "acp-status-running");
      gtk_widget_set_visible (self->spinner, TRUE);
      gtk_spinner_start (GTK_SPINNER (self->spinner));
    }
  else
    {
      if (g_strcmp0 (status, "completed") == 0)
        gtk_widget_add_css_class (self->status, "acp-status-completed");
      else if (g_strcmp0 (status, "failed") == 0)
        gtk_widget_add_css_class (self->status, "acp-status-failed");
      else if (g_strcmp0 (status, "pending") == 0)
        gtk_widget_add_css_class (self->status, "acp-status-pending");

      gtk_spinner_stop (GTK_SPINNER (self->spinner));
      gtk_widget_set_visible (self->spinner, FALSE);
    }
}

static void
gbp_acp_tool_call_row_set_kind (GbpAcpToolCallRow *self,
                                const gchar       *kind)
{
  g_return_if_fail (GBP_IS_ACP_TOOL_CALL_ROW (self));

  g_free (self->kind);
  self->kind = g_strdup (kind ?: "other");

  gtk_image_set_from_icon_name (GTK_IMAGE (self->icon), icon_for_kind (self->kind));
}

static void
append_text_block (GtkWidget   *box,
                   const gchar *text)
{
  GtkWidget *label;

  if (text == NULL || *text == '\0')
    return;

  label = gtk_label_new (text);
  gtk_label_set_wrap (GTK_LABEL (label), TRUE);
  gtk_label_set_selectable (GTK_LABEL (label), TRUE);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0f);
  gtk_widget_add_css_class (label, "monospace");
  gtk_box_append (GTK_BOX (box), label);
}

static void
append_content_item (GbpAcpToolCallRow *self,
                     GtkWidget         *box,
                     GVariant          *item)
{
  const gchar *type = NULL;

  if (item == NULL || !g_variant_is_of_type (item, G_VARIANT_TYPE_VARDICT))
    return;

  if (!g_variant_lookup (item, "type", "&s", &type))
    return;

  if (g_strcmp0 (type, "content") == 0)
    {
      GVariant *content = g_variant_lookup_value (item, "content", G_VARIANT_TYPE_VARDICT);

      if (content != NULL)
        {
          const gchar *inner_type = NULL;
          const gchar *text = NULL;

          if (g_variant_lookup (content, "type", "&s", &inner_type) &&
              g_strcmp0 (inner_type, "text") == 0 &&
              g_variant_lookup (content, "text", "&s", &text))
            append_text_block (box, text);
          else
            append_text_block (box, _("(non-text content)"));

          g_variant_unref (content);
        }
    }
  else if (g_strcmp0 (type, "diff") == 0)
    {
      const gchar *path = NULL;
      const gchar *old_text = NULL;
      const gchar *new_text = NULL;
      GtkWidget *diff;

      if (!g_variant_lookup (item, "path", "&s", &path))
        return;

      g_variant_lookup (item, "oldText", "&s", &old_text);
      g_variant_lookup (item, "newText", "&s", &new_text);

      diff = gbp_acp_diff_view_new (path, old_text, new_text);
      gtk_box_append (GTK_BOX (box), diff);
      self->has_content = TRUE;
    }
  else if (g_strcmp0 (type, "terminal") == 0)
    {
      const gchar *terminal_id = NULL;

      if (g_variant_lookup (item, "terminalId", "&s", &terminal_id))
        {
          g_autofree gchar *text = g_strdup_printf (_("Terminal %s"), terminal_id);
          append_text_block (box, text);
        }
    }
}

static void
gbp_acp_tool_call_row_rebuild_content (GbpAcpToolCallRow *self,
                                       GVariant          *content)
{
  g_autoptr(GVariantIter) iter = NULL;
  GVariant *child;

  g_return_if_fail (GBP_IS_ACP_TOOL_CALL_ROW (self));

  while (gtk_widget_get_first_child (self->content_box) != NULL)
    gtk_box_remove (GTK_BOX (self->content_box),
                    gtk_widget_get_first_child (self->content_box));

  self->has_content = FALSE;

  if (content != NULL && g_variant_is_of_type (content, G_VARIANT_TYPE ("av")))
    {
      iter = g_variant_iter_new (content);

      while (g_variant_iter_loop (iter, "v", &child))
        append_content_item (self, self->content_box, child);
    }

  if (gtk_widget_get_first_child (self->content_box) != NULL)
    {
      self->has_content = TRUE;
      gtk_expander_set_expanded (GTK_EXPANDER (self->expander), TRUE);
    }
}

void
gbp_acp_tool_call_row_update (GbpAcpToolCallRow *self,
                              GVariant          *update)
{
  const gchar *title = NULL;
  const gchar *kind = NULL;
  const gchar *status = NULL;
  GVariant *content;

  g_return_if_fail (GBP_IS_ACP_TOOL_CALL_ROW (self));

  if (update == NULL)
    return;

  if (g_variant_lookup (update, "title", "&s", &title) && title != NULL && *title != '\0')
    gtk_label_set_text (GTK_LABEL (self->title), title);

  if (g_variant_lookup (update, "kind", "&s", &kind) && kind != NULL)
    gbp_acp_tool_call_row_set_kind (self, kind);

  if (g_variant_lookup (update, "status", "&s", &status) && status != NULL)
    gbp_acp_tool_call_row_set_status (self, status);

  content = g_variant_lookup_value (update, "content", NULL);

  if (content != NULL)
    {
      gbp_acp_tool_call_row_rebuild_content (self, content);
      g_variant_unref (content);
    }
}

GtkWidget *
gbp_acp_tool_call_row_new (const gchar *tool_call_id,
                           const gchar *title,
                           const gchar *kind)
{
  GbpAcpToolCallRow *self;
  GtkWidget *header;
  GtkWidget *title_label;
  g_autofree gchar *fallback = NULL;

  g_return_val_if_fail (tool_call_id != NULL, NULL);

  self = g_object_new (GBP_TYPE_ACP_TOOL_CALL_ROW, NULL);
  self->id = g_strdup (tool_call_id);

  header = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);

  self->icon = gtk_image_new ();
  gtk_box_append (GTK_BOX (header), self->icon);

  if (title == NULL || *title == '\0')
    {
      fallback = g_strdup (kind ?: _("Tool call"));
      title = fallback;
    }

  title_label = gtk_label_new (title);
  gtk_label_set_ellipsize (GTK_LABEL (title_label), PANGO_ELLIPSIZE_END);
  gtk_label_set_xalign (GTK_LABEL (title_label), 0.0f);
  gtk_widget_set_hexpand (title_label, TRUE);
  self->title = title_label;
  gtk_box_append (GTK_BOX (header), title_label);

  self->spinner = gtk_spinner_new ();
  gtk_widget_set_visible (self->spinner, FALSE);
  gtk_box_append (GTK_BOX (header), self->spinner);

  self->status = gtk_label_new (NULL);
  gtk_widget_add_css_class (self->status, "caption");
  gtk_widget_add_css_class (self->status, "dim-label");
  gtk_box_append (GTK_BOX (header), self->status);

  self->expander = gtk_expander_new (NULL);
  gtk_expander_set_label_widget (GTK_EXPANDER (self->expander), header);
  gtk_expander_set_expanded (GTK_EXPANDER (self->expander), FALSE);

  self->content_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_widget_set_margin_top (self->content_box, 6);
  gtk_widget_set_margin_start (self->content_box, 12);
  gtk_expander_set_child (GTK_EXPANDER (self->expander), self->content_box);

  gtk_box_append (GTK_BOX (self), self->expander);

  gbp_acp_tool_call_row_set_kind (self, kind);
  gbp_acp_tool_call_row_set_status (self, "pending");

  gtk_widget_add_css_class (GTK_WIDGET (self), "acp-tool-call");

  return GTK_WIDGET (self);
}

const gchar *
gbp_acp_tool_call_row_get_id (GbpAcpToolCallRow *self)
{
  g_return_val_if_fail (GBP_IS_ACP_TOOL_CALL_ROW (self), NULL);
  return self->id;
}
