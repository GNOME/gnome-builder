/* gbp-acp-pane.c
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

#define G_LOG_DOMAIN "gbp-acp-pane"

#include "config.h"

#include <glib/gi18n.h>

#include <libide-code.h>
#include <libide-gui.h>

#include "gbp-acp-client.h"
#include "gbp-acp-message-row.h"
#include "gbp-acp-permission-row.h"
#include "gbp-acp-tool-call-row.h"
#include "gbp-acp-pane.h"

struct _GbpAcpPane
{
  IdePane       parent_instance;

  IdeContext   *context;
  GbpAcpClient *client;
  GCancellable *cancellable;

  GtkWidget    *messages;
  GtkWidget    *scroller;
  GtkWidget    *prompt_view;
  GtkWidget    *send_button;
  GtkWidget    *cancel_button;
  GtkWidget    *spinner;
  GtkWidget    *status_label;
  GtkWidget    *config_box;
  GtkWidget    *commands_button;
  GtkWidget    *commands_list;

  GtkWidget    *current_agent_row;
  GtkWidget    *current_thought_row;
  GtkWidget    *current_permission_row;
  gchar        *current_message_id;
  gchar        *current_thought_id;

  GHashTable   *tool_rows;
  gboolean      updating_config;
};

G_DEFINE_FINAL_TYPE (GbpAcpPane, gbp_acp_pane, IDE_TYPE_PANE)

static void gbp_acp_pane_setup_client (GbpAcpPane *self);

static void
ensure_css (void)
{
  static gboolean loaded;
  g_autoptr(GtkCssProvider) provider = NULL;

  if (loaded)
    return;

  loaded = TRUE;

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_string (provider,
    ".acp-user {"
    "  background-color: alpha(@accent_bg_color, 0.14);"
    "  border-radius: 10px;"
    "}"
    ".acp-agent {"
    "  background-color: alpha(@view_fg_color, 0.06);"
    "  border-radius: 10px;"
    "}"
    ".acp-thought {"
    "  background-color: alpha(@view_fg_color, 0.03);"
    "  border-radius: 10px;"
    "}"
    ".acp-tool-call {"
    "  background-color: alpha(@view_fg_color, 0.04);"
    "  border-radius: 8px;"
    "  padding: 6px;"
    "}"
    ".acp-status-running { color: @accent_color; }"
    ".acp-status-completed { color: @success_color; }"
    ".acp-status-failed { color: @error_color; }"
    ".acp-permission { border: 1px solid alpha(@warning_color, 0.6); }"
    ".acp-plan { padding: 4px; }");

  gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                              GTK_STYLE_PROVIDER (provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

static void
scroll_to_bottom (GbpAcpPane *self)
{
  GtkAdjustment *adj;

  g_assert (GBP_IS_ACP_PANE (self));

  adj = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (self->scroller));

  if (adj != NULL)
    gtk_adjustment_set_value (adj, gtk_adjustment_get_upper (adj));
}

static void
append_row (GbpAcpPane *self,
            GtkWidget  *row)
{
  g_assert (GBP_IS_ACP_PANE (self));
  g_assert (GTK_IS_WIDGET (row));

  gtk_box_append (GTK_BOX (self->messages), row);
  scroll_to_bottom (self);
}

static void
clear_messages (GbpAcpPane *self)
{
  g_assert (GBP_IS_ACP_PANE (self));

  g_hash_table_remove_all (self->tool_rows);

  while (gtk_widget_get_first_child (self->messages) != NULL)
    gtk_box_remove (GTK_BOX (self->messages),
                    gtk_widget_get_first_child (self->messages));

  g_clear_pointer (&self->current_message_id, g_free);
  g_clear_pointer (&self->current_thought_id, g_free);
  self->current_agent_row = NULL;
  self->current_thought_row = NULL;
  self->current_permission_row = NULL;
}

static void
update_actions (GbpAcpPane *self)
{
  gboolean ready;
  gboolean busy;

  g_assert (GBP_IS_ACP_PANE (self));

  ready = self->client != NULL && gbp_acp_client_is_ready (self->client);
  busy = self->client != NULL && gbp_acp_client_is_busy (self->client);

  gtk_widget_set_sensitive (self->send_button, ready && !busy);
  gtk_widget_set_visible (self->cancel_button, busy);
  gtk_widget_set_visible (self->spinner, busy || !ready);

  if (busy || !ready)
    gtk_spinner_start (GTK_SPINNER (self->spinner));
  else
    gtk_spinner_stop (GTK_SPINNER (self->spinner));
}

static void
set_status (GbpAcpPane *self,
            const gchar *text)
{
  gtk_label_set_text (GTK_LABEL (self->status_label), text ?: "");
}

static void
reload_buffer (GbpAcpPane *self,
               const gchar *path)
{
  g_autoptr(GFile) file = NULL;
  IdeBufferManager *buffer_manager;

  g_assert (GBP_IS_ACP_PANE (self));
  g_assert (path != NULL);

  if (self->context == NULL)
    return;

  buffer_manager = ide_buffer_manager_from_context (self->context);
  file = g_file_new_for_path (path);

  if (ide_buffer_manager_find_buffer (buffer_manager, file) != NULL)
    ide_buffer_manager_load_file_async (buffer_manager,
                                        file,
                                        IDE_BUFFER_OPEN_FLAGS_FORCE_RELOAD,
                                        NULL,
                                        NULL,
                                        NULL,
                                        NULL);
}

static void
handle_paths (GbpAcpPane *self,
              GVariant   *update)
{
  g_autoptr(GHashTable) paths = NULL;
  g_autoptr(GVariant) content = NULL;
  g_autoptr(GVariant) locations = NULL;
  const gchar *kind = NULL;
  gboolean modifying = FALSE;
  GHashTableIter iter;
  gpointer key;

  g_assert (GBP_IS_ACP_PANE (self));

  paths = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  if (g_variant_lookup (update, "kind", "&s", &kind))
    modifying = (g_strcmp0 (kind, "edit") == 0 ||
                 g_strcmp0 (kind, "delete") == 0 ||
                 g_strcmp0 (kind, "move") == 0);

  if ((content = g_variant_lookup_value (update, "content", NULL)) != NULL &&
      g_variant_is_of_type (content, G_VARIANT_TYPE ("av")))
    {
      g_autoptr(GVariantIter) content_iter = g_variant_iter_new (content);
      GVariant *child;

      while (g_variant_iter_loop (content_iter, "v", &child))
        {
          const gchar *type = NULL;
          const gchar *path = NULL;

          if (!g_variant_is_of_type (child, G_VARIANT_TYPE_VARDICT))
            continue;
          if (g_variant_lookup (child, "type", "&s", &type) &&
              g_strcmp0 (type, "diff") == 0 &&
              g_variant_lookup (child, "path", "&s", &path))
            g_hash_table_add (paths, g_strdup (path));
        }
    }

  if (modifying &&
      (locations = g_variant_lookup_value (update, "locations", NULL)) != NULL &&
      g_variant_is_of_type (locations, G_VARIANT_TYPE ("av")))
    {
      g_autoptr(GVariantIter) loc_iter = g_variant_iter_new (locations);
      GVariant *child;

      while (g_variant_iter_loop (loc_iter, "v", &child))
        {
          const gchar *path = NULL;

          if (g_variant_is_of_type (child, G_VARIANT_TYPE_VARDICT) &&
              g_variant_lookup (child, "path", "&s", &path))
            g_hash_table_add (paths, g_strdup (path));
        }
    }

  g_hash_table_iter_init (&iter, paths);
  while (g_hash_table_iter_next (&iter, &key, NULL))
    reload_buffer (self, key);
}

static void
append_plan (GbpAcpPane *self,
             GVariant   *update)
{
  g_autoptr(GVariant) entries = NULL;
  g_autoptr(GVariantIter) iter = NULL;
  g_autoptr(GString) text = NULL;
  GtkWidget *row;
  GVariant *child;

  entries = g_variant_lookup_value (update, "entries", NULL);

  if (entries == NULL || !g_variant_is_of_type (entries, G_VARIANT_TYPE ("av")))
    return;

  text = g_string_new (_("Plan:\n"));
  iter = g_variant_iter_new (entries);

  while (g_variant_iter_loop (iter, "v", &child))
    {
      const gchar *content = NULL;
      const gchar *status = NULL;

      if (!g_variant_is_of_type (child, G_VARIANT_TYPE_VARDICT))
        continue;

      g_variant_lookup (child, "content", "&s", &content);
      g_variant_lookup (child, "status", "&s", &status);

      if (content != NULL)
        g_string_append_printf (text, "• %s%s%s\n",
                                content,
                                status ? " (" : "",
                                status ? status : "");
    }

  row = gbp_acp_message_row_new ("agent");
  gbp_acp_message_row_append_text (GBP_ACP_MESSAGE_ROW (row), text->str);
  append_row (self, row);
  self->current_agent_row = NULL;
}

static gchar *
update_dup_text (GVariant *update)
{
  g_autoptr(GVariant) content = NULL;
  const gchar *text = NULL;

  content = g_variant_lookup_value (update, "content", G_VARIANT_TYPE_VARDICT);

  if (content != NULL && g_variant_lookup (content, "text", "&s", &text))
    return g_strdup (text);

  return NULL;
}

static void
append_chunk (GbpAcpPane *self,
              GVariant   *update,
              const gchar *role)
{
  g_autofree gchar *text = NULL;
  const gchar *message_id = NULL;
  GtkWidget **slot;
  gchar **id_slot;
  GtkWidget *row;

  text = update_dup_text (update);

  if (text == NULL || *text == '\0')
    return;

  g_variant_lookup (update, "messageId", "&s", &message_id);

  if (g_strcmp0 (role, "thought") == 0)
    {
      slot = &self->current_thought_row;
      id_slot = &self->current_thought_id;
    }
  else
    {
      slot = &self->current_agent_row;
      id_slot = &self->current_message_id;
    }

  if (*slot == NULL || (message_id != NULL && g_strcmp0 (*id_slot, message_id) != 0))
    {
      row = gbp_acp_message_row_new (role);
      append_row (self, row);
      *slot = row;
      g_free (*id_slot);
      *id_slot = g_strdup (message_id);
    }

  gbp_acp_message_row_append_text (GBP_ACP_MESSAGE_ROW (*slot), text);
  scroll_to_bottom (self);
}

static void
on_tool_call_update (GbpAcpPane *self,
                     GVariant   *update)
{
  const gchar *tool_call_id = NULL;
  GtkWidget *row;

  if (!g_variant_lookup (update, "toolCallId", "&s", &tool_call_id))
    return;

  if ((row = g_hash_table_lookup (self->tool_rows, tool_call_id)) != NULL)
    gbp_acp_tool_call_row_update (GBP_ACP_TOOL_CALL_ROW (row), update);
}

static void
on_tool_call_created (GbpAcpPane *self,
                      GVariant   *update)
{
  const gchar *tool_call_id = NULL;
  const gchar *title = NULL;
  const gchar *kind = NULL;
  GtkWidget *row;

  if (!g_variant_lookup (update, "toolCallId", "&s", &tool_call_id))
    return;

  g_variant_lookup (update, "title", "&s", &title);
  g_variant_lookup (update, "kind", "&s", &kind);

  row = gbp_acp_tool_call_row_new (tool_call_id, title, kind);
  g_hash_table_insert (self->tool_rows, g_strdup (tool_call_id), g_object_ref (row));
  append_row (self, row);
  gbp_acp_tool_call_row_update (GBP_ACP_TOOL_CALL_ROW (row), update);

  self->current_agent_row = NULL;
  self->current_thought_row = NULL;
  g_clear_pointer (&self->current_message_id, g_free);
  g_clear_pointer (&self->current_thought_id, g_free);
}

static gboolean
dict_get_int64 (GVariant    *dict,
                const gchar *key,
                gint64      *out)
{
  g_autoptr(GVariant) value = g_variant_lookup_value (dict, key, NULL);

  if (value == NULL)
    return FALSE;

  if (g_variant_is_of_type (value, G_VARIANT_TYPE_INT64))
    *out = g_variant_get_int64 (value);
  else if (g_variant_is_of_type (value, G_VARIANT_TYPE_INT32))
    *out = g_variant_get_int32 (value);
  else if (g_variant_is_of_type (value, G_VARIANT_TYPE_UINT64))
    *out = (gint64)g_variant_get_uint64 (value);
  else if (g_variant_is_of_type (value, G_VARIANT_TYPE_DOUBLE))
    *out = (gint64)g_variant_get_double (value);
  else
    return FALSE;

  return TRUE;
}

static gboolean
dict_get_double (GVariant    *dict,
                 const gchar *key,
                 gdouble     *out)
{
  g_autoptr(GVariant) value = g_variant_lookup_value (dict, key, NULL);

  if (value == NULL)
    return FALSE;

  if (g_variant_is_of_type (value, G_VARIANT_TYPE_DOUBLE))
    *out = g_variant_get_double (value);
  else if (g_variant_is_of_type (value, G_VARIANT_TYPE_INT64))
    *out = (gdouble)g_variant_get_int64 (value);
  else if (g_variant_is_of_type (value, G_VARIANT_TYPE_INT32))
    *out = (gdouble)g_variant_get_int32 (value);
  else
    return FALSE;

  return TRUE;
}

static void
on_usage_update (GbpAcpPane *self,
                 GVariant   *update)
{
  g_autofree gchar *used = NULL;
  g_autofree gchar *size = NULL;
  g_autofree gchar *text = NULL;
  gint64 used_value = 0;
  gint64 size_value = 0;
  gdouble amount = 0;
  const gchar *currency = NULL;
  g_autoptr(GVariant) cost = NULL;

  dict_get_int64 (update, "used", &used_value);
  dict_get_int64 (update, "size", &size_value);

  used = g_strdup_printf ("%" G_GINT64_FORMAT, used_value);
  size = g_strdup_printf ("%" G_GINT64_FORMAT, size_value);

  cost = g_variant_lookup_value (update, "cost", G_VARIANT_TYPE_VARDICT);

  if (cost != NULL &&
      dict_get_double (cost, "amount", &amount) &&
      g_variant_lookup (cost, "currency", "&s", &currency))
    text = g_strdup_printf (_("%s / %s tokens · %.3f %s"), used, size, amount, currency);
  else
    text = g_strdup_printf (_("%s / %s tokens"), used, size);

  set_status (self, text);
}

static void
on_config_changed_cb (GtkDropDown *dropdown,
                      GParamSpec  *pspec,
                      GbpAcpPane *self)
{
  const gchar *config_id;
  const gchar * const *values;
  guint selected;

  g_assert (GBP_IS_ACP_PANE (self));

  if (self->updating_config || self->client == NULL)
    return;

  selected = gtk_drop_down_get_selected (dropdown);

  if (selected == GTK_INVALID_LIST_POSITION)
    return;

  config_id = g_object_get_data (G_OBJECT (dropdown), "config-id");
  values = g_object_get_data (G_OBJECT (dropdown), "config-values");

  if (config_id != NULL && values != NULL && selected < g_strv_length ((gchar **)values))
    gbp_acp_client_set_config_option (self->client, config_id, values[selected]);
}

static void
rebuild_config (GbpAcpPane *self,
                GVariant   *config_options)
{
  g_autoptr(GVariantIter) iter = NULL;
  GVariant *child;

  g_assert (GBP_IS_ACP_PANE (self));

  self->updating_config = TRUE;

  while (gtk_widget_get_first_child (self->config_box) != NULL)
    gtk_box_remove (GTK_BOX (self->config_box),
                    gtk_widget_get_first_child (self->config_box));

  if (config_options != NULL && g_variant_is_of_type (config_options, G_VARIANT_TYPE ("av")))
    {
      iter = g_variant_iter_new (config_options);

      while (g_variant_iter_loop (iter, "v", &child))
        {
          const gchar *id = NULL;
          const gchar *name = NULL;
          const gchar *type = NULL;
          const gchar *current = NULL;
          GVariant *options = NULL;
          g_autoptr(GtkStringList) model = NULL;
          g_autofree gchar **values = NULL;
          g_autoptr(GVariantIter) options_iter = NULL;
          GtkWidget *dropdown;
          guint n = 0;
          guint current_index = 0;
          GVariant *option;

          if (!g_variant_is_of_type (child, G_VARIANT_TYPE_VARDICT))
            continue;

          g_variant_lookup (child, "id", "&s", &id);
          g_variant_lookup (child, "name", "&s", &name);
          g_variant_lookup (child, "type", "&s", &type);
          g_variant_lookup (child, "currentValue", "&s", &current);

          if (id == NULL || g_strcmp0 (type, "select") != 0)
            continue;

          options = g_variant_lookup_value (child, "options", NULL);

          if (options == NULL || !g_variant_is_of_type (options, G_VARIANT_TYPE ("av")))
            {
              g_clear_pointer (&options, g_variant_unref);
              continue;
            }

          model = gtk_string_list_new (NULL);
          values = g_new0 (gchar *, g_variant_n_children (options) + 1);
          options_iter = g_variant_iter_new (options);

          while (g_variant_iter_loop (options_iter, "v", &option))
            {
              const gchar *value = NULL;
              const gchar *option_name = NULL;

              if (!g_variant_is_of_type (option, G_VARIANT_TYPE_VARDICT))
                continue;

              g_variant_lookup (option, "value", "&s", &value);
              g_variant_lookup (option, "name", "&s", &option_name);

              if (value == NULL)
                continue;

              if (current != NULL && g_strcmp0 (value, current) == 0)
                current_index = n;

              values[n] = g_strdup (value);
              gtk_string_list_append (model, option_name ?: value);
              n++;
            }

          values[n] = NULL;

          dropdown = gtk_drop_down_new (G_LIST_MODEL (model), NULL);
          g_object_set_data_full (G_OBJECT (dropdown), "config-id", g_strdup (id), g_free);
          g_object_set_data_full (G_OBJECT (dropdown), "config-values", g_strdupv (values), (GDestroyNotify)g_strfreev);
          g_signal_connect (dropdown, "notify::selected", G_CALLBACK (on_config_changed_cb), self);

          if (n > 0)
            gtk_drop_down_set_selected (GTK_DROP_DOWN (dropdown), current_index);

          gtk_widget_set_tooltip_text (dropdown, name);
          gtk_box_append (GTK_BOX (self->config_box), dropdown);

          g_clear_pointer (&options, g_variant_unref);
        }
    }

  self->updating_config = FALSE;
}

static void
on_command_clicked_cb (GtkButton  *button,
                       GbpAcpPane *self)
{
  const gchar *command;
  GtkTextBuffer *buffer;
  g_autofree gchar *insert = NULL;

  g_assert (GBP_IS_ACP_PANE (self));

  command = g_object_get_data (G_OBJECT (button), "command");

  if (command == NULL)
    return;

  insert = g_strdup_printf ("/%s ", command);
  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self->prompt_view));
  gtk_text_buffer_set_text (buffer, insert, -1);
  gtk_widget_grab_focus (self->prompt_view);
}

static void
rebuild_commands (GbpAcpPane *self,
                  GVariant   *commands)
{
  g_autoptr(GVariantIter) iter = NULL;
  GVariant *child;

  g_assert (GBP_IS_ACP_PANE (self));

  while (gtk_widget_get_first_child (self->commands_list) != NULL)
    gtk_box_remove (GTK_BOX (self->commands_list),
                    gtk_widget_get_first_child (self->commands_list));

  if (commands == NULL || !g_variant_is_of_type (commands, G_VARIANT_TYPE ("av")))
    return;

  iter = g_variant_iter_new (commands);

  while (g_variant_iter_loop (iter, "v", &child))
    {
      const gchar *name = NULL;
      const gchar *description = NULL;
      GtkWidget *button;
      GtkWidget *label;

      if (!g_variant_is_of_type (child, G_VARIANT_TYPE_VARDICT))
        continue;

      g_variant_lookup (child, "name", "&s", &name);
      g_variant_lookup (child, "description", "&s", &description);

      if (name == NULL)
        continue;

      button = gtk_button_new ();
      gtk_button_set_has_frame (GTK_BUTTON (button), FALSE);
      gtk_widget_add_css_class (button, "flat");
      label = gtk_label_new (name);
      gtk_label_set_xalign (GTK_LABEL (label), 0.0f);
      gtk_button_set_child (GTK_BUTTON (button), label);
      gtk_widget_set_tooltip_text (button, description);
      g_object_set_data_full (G_OBJECT (button), "command", g_strdup (name), g_free);
      g_signal_connect (button, "clicked", G_CALLBACK (on_command_clicked_cb), self);
      gtk_box_append (GTK_BOX (self->commands_list), button);
    }
}

static void
on_client_update (GbpAcpPane *self,
                  GVariant   *update,
                  gpointer    unused)
{
  const gchar *session_update = NULL;

  g_assert (GBP_IS_ACP_PANE (self));
  g_assert (update != NULL);

  if (!g_variant_lookup (update, "sessionUpdate", "&s", &session_update))
    return;

  if (g_strcmp0 (session_update, "agent_message_chunk") == 0)
    append_chunk (self, update, "agent");
  else if (g_strcmp0 (session_update, "user_message_chunk") == 0)
    append_chunk (self, update, "user");
  else if (g_strcmp0 (session_update, "agent_thought_chunk") == 0)
    append_chunk (self, update, "thought");
  else if (g_strcmp0 (session_update, "tool_call") == 0)
    on_tool_call_created (self, update);
  else if (g_strcmp0 (session_update, "tool_call_update") == 0)
    {
      on_tool_call_update (self, update);
      handle_paths (self, update);
    }
  else if (g_strcmp0 (session_update, "plan") == 0)
    append_plan (self, update);
  else if (g_strcmp0 (session_update, "usage_update") == 0)
    on_usage_update (self, update);
  else if (g_strcmp0 (session_update, "available_commands_update") == 0)
    {
      GVariant *commands = g_variant_lookup_value (update, "availableCommands", NULL);
      rebuild_commands (self, commands);
      g_clear_pointer (&commands, g_variant_unref);
    }
  else if (g_strcmp0 (session_update, "config_option_update") == 0)
    {
      GVariant *options = g_variant_lookup_value (update, "configOptions", NULL);
      rebuild_config (self, options);
      g_clear_pointer (&options, g_variant_unref);
    }
}

static void
on_permission_reply (GbpAcpPermissionRow *row,
                     const gchar         *option_id,
                     GbpAcpPane          *self)
{
  g_assert (GBP_IS_ACP_PERMISSION_ROW (row));
  g_assert (option_id != NULL);
  g_assert (GBP_IS_ACP_PANE (self));

  if (self->client != NULL)
    gbp_acp_client_reply_permission (self->client, option_id);

  self->current_permission_row = NULL;
}

static void
on_client_permission (GbpAcpPane *self,
                      GVariant   *tool_call,
                      GVariant   *options,
                      gpointer    unused)
{
  GtkWidget *row;

  g_assert (GBP_IS_ACP_PANE (self));

  row = gbp_acp_permission_row_new (tool_call, options);
  g_signal_connect (row, "reply", G_CALLBACK (on_permission_reply), self);
  append_row (self, row);
  self->current_permission_row = row;
}

static void
on_client_ready (GbpAcpPane *self,
                 gpointer    unused)
{
  g_autoptr(GVariant) options = NULL;
  const gchar *agent_name;

  g_assert (GBP_IS_ACP_PANE (self));

  agent_name = gbp_acp_client_get_agent_name (self->client);

  if (agent_name != NULL)
    set_status (self, agent_name);
  else
    set_status (self, _("Connected"));

  options = gbp_acp_client_dup_config_options (self->client);
  rebuild_config (self, options);
  update_actions (self);
}

static void
on_client_failed (GbpAcpPane *self,
                  GError     *error,
                  gpointer    unused)
{
  GtkWidget *row;
  g_autofree gchar *text = NULL;

  g_assert (GBP_IS_ACP_PANE (self));

  text = g_strdup_printf (_("Agent error: %s"), error ? error->message : _("unknown error"));
  row = gbp_acp_message_row_new ("agent");
  gbp_acp_message_row_append_text (GBP_ACP_MESSAGE_ROW (row), text);
  append_row (self, row);

  set_status (self, _("Disconnected"));
  update_actions (self);
}

static void
on_client_exited (GbpAcpPane *self,
                  gpointer    unused)
{
  g_assert (GBP_IS_ACP_PANE (self));

  set_status (self, _("Agent exited"));
  update_actions (self);
}

static void
on_prompt_finish (GObject      *object,
                  GAsyncResult *result,
                  gpointer      user_data)
{
  g_autoptr(GbpAcpPane) self = user_data;
  g_autoptr(GError) error = NULL;
  g_autofree gchar *stop_reason = NULL;

  g_assert (GBP_IS_ACP_PANE (self));

  stop_reason = gbp_acp_client_prompt_finish (self->client, result, &error);

  if (error != NULL)
    {
      GtkWidget *row = gbp_acp_message_row_new ("agent");
      gbp_acp_message_row_append_text (GBP_ACP_MESSAGE_ROW (row), error->message);
      append_row (self, row);
    }

  if (self->current_agent_row != NULL)
    gbp_acp_message_row_set_complete (GBP_ACP_MESSAGE_ROW (self->current_agent_row), TRUE);
  if (self->current_thought_row != NULL)
    gbp_acp_message_row_set_complete (GBP_ACP_MESSAGE_ROW (self->current_thought_row), TRUE);

  self->current_agent_row = NULL;
  self->current_thought_row = NULL;
  g_clear_pointer (&self->current_message_id, g_free);
  g_clear_pointer (&self->current_thought_id, g_free);

  update_actions (self);
}

static void
send_prompt (GbpAcpPane *self)
{
  GtkTextBuffer *buffer;
  GtkTextIter start;
  GtkTextIter end;
  g_autofree gchar *text = NULL;
  GtkWidget *row;

  g_assert (GBP_IS_ACP_PANE (self));

  if (self->client == NULL || !gbp_acp_client_is_ready (self->client))
    return;

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self->prompt_view));
  gtk_text_buffer_get_bounds (buffer, &start, &end);
  text = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);
  g_strstrip (text);

  if (*text == '\0')
    return;

  row = gbp_acp_message_row_new ("user");
  gbp_acp_message_row_append_text (GBP_ACP_MESSAGE_ROW (row), text);
  append_row (self, row);

  self->current_agent_row = NULL;
  self->current_thought_row = NULL;

  gtk_text_buffer_set_text (buffer, "", 0);

  update_actions (self);

  gbp_acp_client_prompt_async (self->client,
                               text,
                               self->cancellable,
                               on_prompt_finish,
                               g_object_ref (self));
}

static void
on_send_clicked_cb (GtkButton  *button,
                    GbpAcpPane *self)
{
  send_prompt (self);
}

static void
on_cancel_clicked_cb (GtkButton  *button,
                      GbpAcpPane *self)
{
  if (self->client != NULL)
    gbp_acp_client_cancel (self->client);
}

static void
on_new_session_clicked_cb (GtkButton  *button,
                           GbpAcpPane *self)
{
  g_assert (GBP_IS_ACP_PANE (self));

  if (self->client != NULL)
    {
      gbp_acp_client_stop (self->client);
      g_clear_object (&self->client);
    }

  clear_messages (self);
  set_status (self, _("Starting agent…"));
  gbp_acp_pane_setup_client (self);
}

static gboolean
on_prompt_key_pressed_cb (GtkEventControllerKey *controller,
                          guint                  keyval,
                          guint                  keycode,
                          GdkModifierType        state,
                          GbpAcpPane            *self)
{
  g_assert (GBP_IS_ACP_PANE (self));

  if ((keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter) &&
      (state & GDK_SHIFT_MASK) == 0)
    {
      send_prompt (self);
      return GDK_EVENT_STOP;
    }

  return GDK_EVENT_PROPAGATE;
}

static void
gbp_acp_pane_setup_client (GbpAcpPane *self)
{
  g_assert (GBP_IS_ACP_PANE (self));

  self->client = gbp_acp_client_new (self->context);

  g_signal_connect_object (self->client, "update",
                           G_CALLBACK (on_client_update), self, G_CONNECT_SWAPPED);
  g_signal_connect_object (self->client, "permission",
                           G_CALLBACK (on_client_permission), self, G_CONNECT_SWAPPED);
  g_signal_connect_object (self->client, "ready",
                           G_CALLBACK (on_client_ready), self, G_CONNECT_SWAPPED);
  g_signal_connect_object (self->client, "failed",
                           G_CALLBACK (on_client_failed), self, G_CONNECT_SWAPPED);
  g_signal_connect_object (self->client, "exited",
                           G_CALLBACK (on_client_exited), self, G_CONNECT_SWAPPED);

  gbp_acp_client_start (self->client);
  update_actions (self);
}

static GtkWidget *
build_ui (GbpAcpPane *self)
{
  GtkWidget *root;
  GtkWidget *toolbar;
  GtkWidget *title;
  GtkWidget *spacer;
  GtkWidget *new_button;
  GtkWidget *config_row;
  GtkWidget *prompt_frame;
  GtkWidget *prompt_scroller;
  GtkWidget *prompt_box;
  GtkWidget *send_button;
  GtkWidget *cancel_button;
  GtkWidget *commands_popover;
  GtkWidget *commands_scroller;
  GtkWidget *commands_box;
  GtkEventController *key_controller;

  ensure_css ();

  root = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

  /* Toolbar */
  toolbar = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_widget_set_margin_start (toolbar, 6);
  gtk_widget_set_margin_end (toolbar, 6);
  gtk_widget_set_margin_top (toolbar, 6);
  gtk_widget_set_margin_bottom (toolbar, 6);

  title = gtk_label_new (_("Agent"));
  gtk_widget_add_css_class (title, "heading");
  gtk_box_append (GTK_BOX (toolbar), title);

  self->spinner = gtk_spinner_new ();
  gtk_box_append (GTK_BOX (toolbar), self->spinner);

  spacer = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_hexpand (spacer, TRUE);
  gtk_box_append (GTK_BOX (toolbar), spacer);

  self->status_label = gtk_label_new (_("Starting agent…"));
  gtk_label_set_ellipsize (GTK_LABEL (self->status_label), PANGO_ELLIPSIZE_END);
  gtk_widget_add_css_class (self->status_label, "caption");
  gtk_widget_add_css_class (self->status_label, "dim-label");
  gtk_box_append (GTK_BOX (toolbar), self->status_label);

  new_button = gtk_button_new_from_icon_name ("view-refresh-symbolic");
  gtk_widget_set_tooltip_text (new_button, _("Start a new session"));
  gtk_widget_add_css_class (new_button, "flat");
  g_signal_connect (new_button, "clicked", G_CALLBACK (on_new_session_clicked_cb), self);
  gtk_box_append (GTK_BOX (toolbar), new_button);

  gtk_box_append (GTK_BOX (root), toolbar);

  /* Config options + commands */
  config_row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_widget_set_margin_start (config_row, 6);
  gtk_widget_set_margin_end (config_row, 6);
  gtk_widget_set_margin_bottom (config_row, 6);

  self->config_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_widget_set_hexpand (self->config_box, TRUE);
  gtk_box_append (GTK_BOX (config_row), self->config_box);

  self->commands_button = gtk_menu_button_new ();
  gtk_menu_button_set_icon_name (GTK_MENU_BUTTON (self->commands_button), "system-run-symbolic");
  gtk_widget_set_tooltip_text (self->commands_button, _("Slash commands"));
  gtk_widget_add_css_class (self->commands_button, "flat");

  commands_popover = gtk_popover_new ();
  commands_scroller = gtk_scrolled_window_new ();
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (commands_scroller),
                                  GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_max_content_height (GTK_SCROLLED_WINDOW (commands_scroller), 320);
  gtk_scrolled_window_set_propagate_natural_height (GTK_SCROLLED_WINDOW (commands_scroller), TRUE);
  gtk_widget_set_size_request (commands_scroller, 320, -1);
  self->commands_list = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (commands_scroller), self->commands_list);
  gtk_popover_set_child (GTK_POPOVER (commands_popover), commands_scroller);
  gtk_menu_button_set_popover (GTK_MENU_BUTTON (self->commands_button), commands_popover);
  gtk_box_append (GTK_BOX (config_row), self->commands_button);

  gtk_box_append (GTK_BOX (root), config_row);

  /* Messages */
  self->messages = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
  gtk_widget_set_margin_top (self->messages, 6);
  gtk_widget_set_margin_bottom (self->messages, 6);

  self->scroller = gtk_scrolled_window_new ();
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (self->scroller),
                                  GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (self->scroller), self->messages);
  gtk_widget_set_vexpand (self->scroller, TRUE);
  gtk_box_append (GTK_BOX (root), self->scroller);

  /* Prompt */
  prompt_frame = gtk_frame_new (NULL);
  gtk_widget_set_margin_start (prompt_frame, 6);
  gtk_widget_set_margin_end (prompt_frame, 6);
  gtk_widget_set_margin_bottom (prompt_frame, 6);

  prompt_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_widget_set_margin_start (prompt_box, 6);
  gtk_widget_set_margin_end (prompt_box, 6);
  gtk_widget_set_margin_top (prompt_box, 6);
  gtk_widget_set_margin_bottom (prompt_box, 6);

  self->prompt_view = gtk_text_view_new ();
  gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (self->prompt_view), GTK_WRAP_WORD_CHAR);
  gtk_text_view_set_accepts_tab (GTK_TEXT_VIEW (self->prompt_view), FALSE);
  gtk_widget_set_valign (self->prompt_view, GTK_ALIGN_CENTER);
  key_controller = gtk_event_controller_key_new ();
  g_signal_connect (key_controller, "key-pressed",
                    G_CALLBACK (on_prompt_key_pressed_cb), self);
  gtk_widget_add_controller (self->prompt_view, key_controller);

  prompt_scroller = gtk_scrolled_window_new ();
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (prompt_scroller),
                                  GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_max_content_height (GTK_SCROLLED_WINDOW (prompt_scroller), 140);
  gtk_scrolled_window_set_propagate_natural_height (GTK_SCROLLED_WINDOW (prompt_scroller), TRUE);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (prompt_scroller), self->prompt_view);
  gtk_widget_set_hexpand (prompt_scroller, TRUE);
  gtk_box_append (GTK_BOX (prompt_box), prompt_scroller);

  cancel_button = gtk_button_new_from_icon_name ("process-stop-symbolic");
  gtk_widget_set_tooltip_text (cancel_button, _("Cancel"));
  gtk_widget_add_css_class (cancel_button, "destructive-action");
  gtk_widget_set_visible (cancel_button, FALSE);
  g_signal_connect (cancel_button, "clicked", G_CALLBACK (on_cancel_clicked_cb), self);
  gtk_box_append (GTK_BOX (prompt_box), cancel_button);
  self->cancel_button = cancel_button;

  send_button = gtk_button_new_from_icon_name ("document-send-symbolic");
  gtk_widget_set_tooltip_text (send_button, _("Send"));
  gtk_widget_add_css_class (send_button, "suggested-action");
  g_signal_connect (send_button, "clicked", G_CALLBACK (on_send_clicked_cb), self);
  gtk_box_append (GTK_BOX (prompt_box), send_button);
  self->send_button = send_button;

  gtk_frame_set_child (GTK_FRAME (prompt_frame), prompt_box);
  gtk_box_append (GTK_BOX (root), prompt_frame);

  return root;
}

static void
gbp_acp_pane_dispose (GObject *object)
{
  GbpAcpPane *self = (GbpAcpPane *)object;

  if (self->cancellable != NULL)
    g_cancellable_cancel (self->cancellable);

  g_clear_object (&self->cancellable);
  g_clear_object (&self->client);
  g_clear_object (&self->context);
  g_clear_pointer (&self->tool_rows, g_hash_table_unref);
  g_clear_pointer (&self->current_message_id, g_free);
  g_clear_pointer (&self->current_thought_id, g_free);

  G_OBJECT_CLASS (gbp_acp_pane_parent_class)->dispose (object);
}

static void
gbp_acp_pane_class_init (GbpAcpPaneClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gbp_acp_pane_dispose;
}

static void
gbp_acp_pane_init (GbpAcpPane *self)
{
  GtkWidget *root;

  self->cancellable = g_cancellable_new ();
  self->tool_rows = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);

  root = build_ui (self);
  panel_widget_set_child (PANEL_WIDGET (self), root);
}

GbpAcpPane *
gbp_acp_pane_new (IdeContext *context)
{
  GbpAcpPane *self;

  g_return_val_if_fail (IDE_IS_CONTEXT (context), NULL);

  self = g_object_new (GBP_TYPE_ACP_PANE, NULL);
  self->context = g_object_ref (context);

  gbp_acp_pane_setup_client (self);

  return self;
}
