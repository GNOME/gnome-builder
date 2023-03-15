/* ide-shortcut-bundle.c
 *
 * Copyright 2022-2023 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-shortcut-bundle"

#include "config.h"

#include <gtk/gtk.h>
#include <json-glib/json-glib.h>
#include <tmpl-glib.h>

#include "ide-gui-global.h"
#include "ide-gui-resources.h"
#include "ide-shortcut-bundle-private.h"
#include "ide-workbench.h"
#include "ide-workspace.h"

struct _IdeShortcutBundle
{
  GObject       parent_instance;
  GPtrArray    *items;
  GError       *error;
  GFile        *file;
  GFileMonitor *file_monitor;
  guint         reload_source;
  guint         is_user : 1;
};

static TmplScope *imports_scope;

static IdeShortcut *
ide_shortcut_new (const char          *id,
                  const char          *override,
                  const char          *action,
                  GVariant            *args,
                  TmplExpr            *when,
                  GtkPropagationPhase  phase)
{
  IdeShortcut *ret;

  g_assert (id == NULL || override == NULL);
  g_assert (phase == GTK_PHASE_CAPTURE || phase == GTK_PHASE_BUBBLE);
  g_assert (action != NULL || override != NULL);

  ret = g_slice_new0 (IdeShortcut);
  ret->id = g_strdup (id);
  ret->override = g_strdup (override);
  if (action != NULL)
    ret->action = gtk_named_action_new (action);
  ret->args = args ? g_variant_ref_sink (args) : NULL;
  ret->when = when ? tmpl_expr_ref (when) : NULL;
  ret->phase = phase;

  return ret;
}

static IdeShortcut *
ide_shortcut_new_suppress (TmplExpr            *when,
                           GtkPropagationPhase  phase)
{
  IdeShortcut *ret;

  g_assert (phase == GTK_PHASE_CAPTURE || phase == GTK_PHASE_BUBBLE);

  ret = g_slice_new0 (IdeShortcut);
  ret->action = g_object_ref (gtk_nothing_action_get ());
  ret->args = NULL;
  ret->when = when ? tmpl_expr_ref (when) : NULL;
  ret->phase = phase;

  return ret;
}

static void
ide_shortcut_free (IdeShortcut *shortcut)
{
  g_clear_pointer (&shortcut->id, g_free);
  g_clear_pointer (&shortcut->override, g_free);
  g_clear_pointer (&shortcut->when, tmpl_expr_unref);
  g_clear_pointer (&shortcut->args, g_variant_unref);
  g_clear_object (&shortcut->action);
  g_clear_object (&shortcut->trigger);
  shortcut->phase = 0;
  g_slice_free (IdeShortcut, shortcut);
}

static void
set_object (TmplScope  *scope,
            const char *name,
            GType       type,
            gpointer    object)
{
  if (object != NULL)
    tmpl_scope_set_object (scope, name, object);
  else
    tmpl_scope_set_null (scope, name);
}

static gboolean
ide_shortcut_activate (GtkWidget *widget,
                       GVariant  *args,
                       gpointer   user_data)
{
  IdeShortcut *shortcut = user_data;
  GtkWidget *focus = NULL;

  g_assert (GTK_IS_WIDGET (widget));
  g_assert (shortcut != NULL);

  /* Never activate if this is an override. We want the shortcut to activate
   * from the original position so that it applies the same "when" and "phase"
   * as the original shortcut this overrides.
   *
   * Bundles can bind their trigger to the shortcut manager to have their
   * trigger updated in response to user bundle changes.
   */
  if (shortcut->override != NULL)
    return FALSE;

  if (shortcut->when != NULL)
    {
      g_autoptr(TmplScope) scope = tmpl_scope_new_with_parent (imports_scope);
      g_autoptr(GError) error = NULL;
      g_auto(GValue) enabled = G_VALUE_INIT;

      IdeWorkspace *workspace = ide_widget_get_workspace (widget);
      IdeWorkbench *workbench = ide_widget_get_workbench (widget);
      IdePage *page = workspace ? ide_workspace_get_most_recent_page (workspace) : NULL;

      if (GTK_IS_ROOT (widget))
        focus = gtk_root_get_focus (GTK_ROOT (widget));

      if (focus == NULL)
        focus = widget;

      set_object (scope, "focus", GTK_TYPE_WIDGET, focus);
      set_object (scope, "workbench", IDE_TYPE_WORKBENCH, workbench);
      set_object (scope, "workspace", IDE_TYPE_WORKSPACE, workspace);
      set_object (scope, "page", IDE_TYPE_PAGE, page);

      if (!tmpl_expr_eval (shortcut->when, scope, &enabled, &error))
        {
          g_warning ("Failure to eval \"when\": %s", error->message);
          return FALSE;
        }

      if (!G_VALUE_HOLDS_BOOLEAN (&enabled))
        {
          GValue as_bool = G_VALUE_INIT;

          g_value_init (&as_bool, G_TYPE_BOOLEAN);
          if (!g_value_transform (&enabled, &as_bool))
            return FALSE;

          g_value_unset (&enabled);
          enabled = as_bool;
        }

      g_assert (G_VALUE_HOLDS_BOOLEAN (&enabled));

      if (!g_value_get_boolean (&enabled))
        return FALSE;
    }

  if (GTK_IS_NOTHING_ACTION (shortcut->action))
    return TRUE;

  return gtk_shortcut_action_activate (shortcut->action,
                                       GTK_SHORTCUT_ACTION_EXCLUSIVE,
                                       focus ? focus : widget,
                                       shortcut->args);
}

static guint
ide_shortcut_bundle_get_n_items (GListModel *model)
{
  IdeShortcutBundle *self = IDE_SHORTCUT_BUNDLE (model);

  return self->items ? self->items->len : 0;
}

static gpointer
ide_shortcut_bundle_get_item (GListModel *model,
                              guint       position)
{
  IdeShortcutBundle *self = IDE_SHORTCUT_BUNDLE (model);

  if (self->items == NULL || position >= self->items->len)
    return NULL;

  return g_object_ref (g_ptr_array_index (self->items, position));
}

static GType
ide_shortcut_bundle_get_item_type (GListModel *model)
{
  return GTK_TYPE_SHORTCUT;
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_n_items = ide_shortcut_bundle_get_n_items;
  iface->get_item = ide_shortcut_bundle_get_item;
  iface->get_item_type = ide_shortcut_bundle_get_item_type;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (IdeShortcutBundle, ide_shortcut_bundle, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

static void
ide_shortcut_bundle_dispose (GObject *object)
{
  IdeShortcutBundle *self = (IdeShortcutBundle *)object;

  g_clear_object (&self->file);
  g_clear_object (&self->file_monitor);
  g_clear_pointer (&self->items, g_ptr_array_unref);
  g_clear_error (&self->error);

  g_clear_handle_id (&self->reload_source, g_source_remove);

  G_OBJECT_CLASS (ide_shortcut_bundle_parent_class)->dispose (object);
}

static void
ide_shortcut_bundle_class_init (IdeShortcutBundleClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_shortcut_bundle_dispose;

  g_resources_register (ide_gui_get_resource ());
}

static void
ide_shortcut_bundle_init (IdeShortcutBundle *self)
{
  if (g_once_init_enter (&imports_scope))
    {
      g_autoptr(GBytes) bytes = g_resources_lookup_data ("/org/gnome/libide-gui/gtk/keybindings.gsl", 0, NULL);
      const char *str = (const char *)g_bytes_get_data (bytes, NULL);
      g_autoptr(TmplExpr) expr = NULL;
      g_autoptr(GError) error = NULL;
      g_auto(GValue) return_value = G_VALUE_INIT;
      TmplScope *scope = tmpl_scope_new ();

      if (!(expr = tmpl_expr_from_string (str, &error)))
        g_critical ("Failed to parse keybindings.gsl: %s", error->message);
      else if (!tmpl_expr_eval (expr, scope, &return_value, &error))
        g_critical ("Failed to eval keybindings.gsl: %s", error->message);

      g_once_init_leave (&imports_scope, scope);
    }

  self->items = g_ptr_array_new_with_free_func (g_object_unref);
}

IdeShortcutBundle *
ide_shortcut_bundle_new (void)
{
  return g_object_new (IDE_TYPE_SHORTCUT_BUNDLE, NULL);
}

static gboolean
get_boolean_member (JsonObject  *obj,
                    const char  *name,
                    gboolean    *value,
                    GError     **error)
{
  JsonNode *node;

  *value = FALSE;

  if (!json_object_has_member (obj, name))
    return TRUE;

  node = json_object_get_member (obj, name);

  if (!JSON_NODE_HOLDS_VALUE (node))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_DATA,
                   "Key \"%s\" contains something other than a string",
                   name);
      return FALSE;
    }

  *value = json_node_get_boolean (node);

  return TRUE;
}

static gboolean
get_string_member (JsonObject  *obj,
                   const char  *name,
                   const char **value,
                   GError     **error)
{
  JsonNode *node;
  const char *str;

  *value = NULL;

  if (!json_object_has_member (obj, name))
    return TRUE;

  node = json_object_get_member (obj, name);

  if (!JSON_NODE_HOLDS_VALUE (node))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_DATA,
                   "Key \"%s\" contains something other than a string",
                   name);
      return FALSE;
    }

  str = json_node_get_string (node);

  if (str != NULL && strlen (str) > 1024)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_DATA,
                   "Implausible string found, bailing. Length %"G_GSIZE_FORMAT,
                   strlen (str));
      return FALSE;
    }

  *value = g_intern_string (str);

  return TRUE;
}

static gboolean
parse_phase (const char          *str,
             GtkPropagationPhase *phase)
{
  if (str == NULL || str[0] == 0 || strcasecmp ("capture", str) == 0)
    {
      *phase = GTK_PHASE_CAPTURE;
      return TRUE;
    }
  else if (strcasecmp ("bubble", str) == 0)
    {
      *phase = GTK_PHASE_BUBBLE;
      return TRUE;
    }

  return FALSE;
}

static gboolean
populate_from_object (IdeShortcutBundle  *self,
                      JsonNode           *node,
                      GError            **error)
{
  g_autoptr(GtkShortcutTrigger) trigger = NULL;
  g_autoptr(GtkShortcutAction) callback = NULL;
  g_autoptr(GtkShortcut) shortcut = NULL;
  g_autoptr(TmplExpr) when = NULL;
  g_autoptr(GVariant) args = NULL;
  const char *trigger_str = NULL;
  const char *id_str = NULL;
  const char *override_str = NULL;
  const char *when_str = NULL;
  const char *args_str = NULL;
  const char *phase_str = NULL;
  const char *command = NULL;
  const char *action = NULL;
  IdeShortcut *state;
  GtkPropagationPhase phase = 0;
  JsonObject *obj;
  gboolean suppress = FALSE;

  g_assert (IDE_IS_SHORTCUT_BUNDLE (self));
  g_assert (node != NULL);
  g_assert (JSON_NODE_HOLDS_OBJECT (node));

  obj = json_node_get_object (node);

  /* TODO: We might want to add title/description so that our internal
   *       keybindings can be displayed to the user from global search
   *       with more than just a command name and/or arguments.
   */

  if (!get_string_member (obj, "trigger", &trigger_str, error) ||
      !get_string_member (obj, "id", &id_str, error) ||
      !get_string_member (obj, "override", &override_str, error) ||
      !get_string_member (obj, "when", &when_str, error) ||
      !get_string_member (obj, "args", &args_str, error) ||
      !get_string_member (obj, "command", &command, error) ||
      !get_string_member (obj, "action", &action, error) ||
      !get_string_member (obj, "phase", &phase_str, error) ||
      !get_boolean_member (obj, "suppress", &suppress, error))
    return FALSE;

  if (ide_str_empty0 (trigger_str))
    {
      trigger = g_object_ref (gtk_never_trigger_get ());
    }
  else if (!(trigger = gtk_shortcut_trigger_parse_string (trigger_str)))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_DATA,
                   "Failed to parse shortcut trigger: \"%s\"",
                   trigger_str);
      return FALSE;
    }

  if (id_str != NULL && suppress == TRUE)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_DATA,
                   "\"id\" and \"supress\" may not both be set");
      return FALSE;
    }

  if (id_str != NULL && override_str != NULL)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_DATA,
                   "\"id\" and \"override\" may not both be set");
      return FALSE;
    }

  if (suppress)
    goto do_parse_when;

  if (!ide_str_empty0 (command) && !ide_str_empty0 (action))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_DATA,
                   "Cannot specify both \"command\" and \"action\" (\"%s\" and \"%s\")",
                   command, action);
      return FALSE;
    }

  if (!ide_str_empty0 (args_str))
    {
      /* Parse args from string into GVariant if any */
      if (!(args = g_variant_parse (NULL, args_str, NULL, NULL, error)))
        return FALSE;
    }

  if (!ide_str_empty0 (command))
    {
      GVariantBuilder builder;

      g_variant_builder_init (&builder, G_VARIANT_TYPE ("(smv)"));
      g_variant_builder_add (&builder, "s", command);
      g_variant_builder_open (&builder, G_VARIANT_TYPE ("mv"));
      if (args != NULL)
        g_variant_builder_add_value (&builder, args);
      g_variant_builder_close (&builder);

      g_clear_pointer (&args, g_variant_unref);
      args = g_variant_builder_end (&builder);
      action = "context.workbench.command";
    }

do_parse_when:
  if (!ide_str_empty0 (when_str))
    {
      if (!(when = tmpl_expr_from_string (when_str, error)))
        return FALSE;
    }

  if (!parse_phase (phase_str, &phase))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_DATA,
                   "Unknown phase \"%s\"",
                   phase_str);
      return FALSE;
    }

  if (suppress)
    state = ide_shortcut_new_suppress (when, phase);
  else
    state = ide_shortcut_new (id_str, override_str, action, args, when, phase);

  /* Keep a copy of the original trigger around so that if we override the
   * shortcut's trigger from a user-bundle override, we can reset it if that
   * shortcut gets removed.
   */
  g_set_object (&state->trigger, trigger);

  callback = gtk_callback_action_new (ide_shortcut_activate, state,
                                      (GDestroyNotify) ide_shortcut_free);
  shortcut = gtk_shortcut_new (g_steal_pointer (&trigger),
                               g_steal_pointer (&callback));
  g_object_set_data (G_OBJECT (shortcut), "PHASE", GINT_TO_POINTER (phase));
  g_object_set_data (G_OBJECT (shortcut), "IDE_SHORTCUT", state);
  g_ptr_array_add (self->items, g_steal_pointer (&shortcut));

  return TRUE;
}

static gboolean
populate_from_array (IdeShortcutBundle  *self,
                     JsonNode           *node,
                     GError            **error)
{
  JsonArray *ar;
  guint n_items;

  g_assert (IDE_IS_SHORTCUT_BUNDLE (self));
  g_assert (node != NULL);
  g_assert (JSON_NODE_HOLDS_ARRAY (node));

  ar = json_node_get_array (node);
  n_items = json_array_get_length (ar);

  for (guint i = 0; i < n_items; i++)
    {
      JsonNode *element = json_array_get_element (ar, i);

      if (JSON_NODE_HOLDS_ARRAY (element))
        {
          if (!populate_from_array (self, element, error))
            return FALSE;
        }
      else if (JSON_NODE_HOLDS_OBJECT (element))
        {
          if (!populate_from_object (self, element, error))
            return FALSE;
        }
      else
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_INVALID_DATA,
                       "Something other than an object found within array");
          return FALSE;
        }
    }

  return TRUE;
}

static gboolean
ide_shortcut_bundle_parse_internal (IdeShortcutBundle  *self,
                                    GFile              *file,
                                    GError            **error)
{
  g_autoptr(JsonParser) parser = NULL;
  g_autofree char *data = NULL;
  g_autofree char *expanded = NULL;
  JsonNode *root;
  gsize len = 0;

  g_assert (IDE_IS_SHORTCUT_BUNDLE (self));
  g_assert (G_IS_FILE (file));

  /* @data is always \0 terminated by g_file_load_contents() */
  if (!g_file_load_contents (file, NULL, &data, &len, NULL, error))
    return FALSE;

  /* We sort of want to look like keybindings.json style, which could
   * mean some munging for trailing , and missing [].
   */
  g_strstrip (data);
  len = strlen (data);
  if (len > 0 && data[len-1] == ',')
    data[len-1] = 0;
  expanded = g_strdup_printf ("[%s]", data);

  parser = json_parser_new ();
  if (!json_parser_load_from_data (parser, expanded, -1, error))
    return FALSE;

  /* Nothing to do if the contents are empty */
  if (!(root = json_parser_get_root (parser)))
    return TRUE;

  /* In case we get arrays containing arrays, try to handle them gracefully
   * and unscrew this terribly defined file format by VSCode.
   */
  if (JSON_NODE_HOLDS_ARRAY (root))
    return populate_from_array (self, root, error);
  else if (JSON_NODE_HOLDS_OBJECT (root))
    return populate_from_object (self, root, error);

  g_set_error (error,
               G_IO_ERROR,
               G_IO_ERROR_INVALID_DATA,
               "Got something other than an array or object");

  return FALSE;
}

gboolean
ide_shortcut_bundle_parse (IdeShortcutBundle  *self,
                           GFile              *file,
                           GError            **error)
{
  gboolean ret;

  g_return_val_if_fail (IDE_IS_SHORTCUT_BUNDLE (self), FALSE);
  g_return_val_if_fail (G_IS_FILE (file), FALSE);

  g_clear_error (&self->error);
  ret = ide_shortcut_bundle_parse_internal (self, file, &self->error);
  if (self->error && error != NULL)
    g_propagate_error (error, g_error_copy (self->error));
  return ret;
}

const GError *
ide_shortcut_bundle_error (IdeShortcutBundle *self)
{
  g_return_val_if_fail (IDE_IS_SHORTCUT_BUNDLE (self), NULL);

  return self->error;
}

gboolean
ide_shortcut_is_phase (GtkShortcut         *shortcut,
                       GtkPropagationPhase  phase)
{
  g_return_val_if_fail (GTK_IS_SHORTCUT (shortcut), FALSE);

  return g_object_get_data (G_OBJECT (shortcut), "PHASE") == GUINT_TO_POINTER (phase);
}

gboolean
ide_shortcut_is_suppress (GtkShortcut *shortcut)
{
  IdeShortcut *state;

  g_return_val_if_fail (GTK_IS_SHORTCUT (shortcut), FALSE);

  if ((state = g_object_get_data (G_OBJECT (shortcut), "IDE_SHORTCUT")))
    return GTK_IS_NOTHING_ACTION (state->action);

  return FALSE;
}

static gboolean
ide_shortcut_bundle_do_reload (IdeShortcutBundle *self)
{
  g_assert (IDE_IS_SHORTCUT_BUNDLE (self));

  self->reload_source = 0;

  if (self->items->len > 0)
    {
      guint len = self->items->len;

      g_ptr_array_remove_range (self->items, 0, len);
      g_list_model_items_changed (G_LIST_MODEL (self), 0, len, 0);
    }

  g_clear_error (&self->error);

  if (g_file_query_exists (self->file, NULL) &&
      !ide_shortcut_bundle_parse (self, self->file, &self->error))
    g_warning ("Failed to parse %s: %s",
               g_file_peek_path (self->file),
               self->error->message);

  if (self->items->len > 0)
    g_list_model_items_changed (G_LIST_MODEL (self), 0, 0, self->items->len);

  return G_SOURCE_REMOVE;
}

static void
ide_shortcut_bundle_queue_reload (IdeShortcutBundle *self)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_SHORTCUT_BUNDLE (self));

  if (self->reload_source == 0)
    self->reload_source = g_idle_add_full (G_PRIORITY_LOW,
                                           (GSourceFunc)ide_shortcut_bundle_do_reload,
                                           self, NULL);
}

static void
on_file_monitor_changed_cb (IdeShortcutBundle *self,
                            GFile             *file,
                            GFile             *other_file,
                            GFileMonitorEvent  event,
                            GFileMonitor      *monitor)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_SHORTCUT_BUNDLE (self));
  g_assert (G_IS_FILE (file));
  g_assert (G_IS_FILE_MONITOR (monitor));

  ide_shortcut_bundle_queue_reload (self);
}

IdeShortcutBundle *
ide_shortcut_bundle_new_for_user (GFile *file)
{
  IdeShortcutBundle *self;

  g_return_val_if_fail (G_IS_FILE (file), NULL);

  self = ide_shortcut_bundle_new ();

  g_debug ("Looking for user shortcuts at \"%s\"",
           g_file_peek_path (file));

  self->file = g_object_ref (file);
  self->file_monitor = g_file_monitor (file, G_FILE_MONITOR_NONE, NULL, NULL);
  self->is_user = TRUE;

  g_signal_connect_object (self->file_monitor,
                           "changed",
                           G_CALLBACK (on_file_monitor_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);
  on_file_monitor_changed_cb (self,
                              file,
                              NULL,
                              G_FILE_MONITOR_EVENT_CHANGED,
                              self->file_monitor);

  return self;
}

void
ide_shortcut_bundle_override_triggers (IdeShortcutBundle *self,
                                       GHashTable        *id_to_trigger)
{
  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_SHORTCUT_BUNDLE (self));
  g_return_if_fail (id_to_trigger != NULL);

  /* This function will see if we can override the GtkShortcut:trigger for
   * each of the GtkShortcut in the bundle. Generally @id_to_trigger should
   * have been made by consuming the overrides in the user bundle.
   */

  for (guint i = 0; i < self->items->len; i++)
    {
      GtkShortcut *shortcut = g_ptr_array_index (self->items, i);
      IdeShortcut *state = g_object_get_data (G_OBJECT (shortcut), "IDE_SHORTCUT");
      GtkShortcutTrigger *trigger;

      g_assert (GTK_IS_SHORTCUT (shortcut));
      g_assert (state != NULL);

      if (state->id == NULL)
        continue;

      trigger = g_hash_table_lookup (id_to_trigger, state->id);

      if (trigger == NULL)
        {
          if (state->trigger == NULL)
            trigger = gtk_never_trigger_get ();
          else
            trigger = state->trigger;
        }

      g_assert (trigger != NULL);
      g_assert (GTK_IS_SHORTCUT_TRIGGER (trigger));

      gtk_shortcut_set_trigger (shortcut, g_object_ref (trigger));
    }
}

static void
copy_object (JsonObject *object,
             GString    *output)
{
  static const char *str_keys[] = { "id", "override", "trigger", "when", "args", "phase", "command", "action", };
  static const char *bool_keys[] = { "supress" };

  g_assert (object != NULL);
  g_assert (output != NULL);

  if (json_object_get_size (object) == 0)
    return;

  g_string_append (output, "{");

  for (guint i = 0; i < G_N_ELEMENTS (str_keys); i++)
    {
      const char *value;

      if (!json_object_has_member (object, str_keys[i]))
        continue;

      if (!(value = json_object_get_string_member (object, str_keys[i])))
        continue;

      if (ide_str_empty0 (value))
        continue;

      if (output->str[output->len-1] != '{')
        g_string_append_c (output, ',');

      g_string_append_printf (output, " \"%s\" : \"%s\"", str_keys[i], value);
    }

  for (guint i = 0; i < G_N_ELEMENTS (bool_keys); i++)
    {
      gboolean value;

      if (!json_object_has_member (object, bool_keys[i]))
        continue;

      value = json_object_get_boolean_member (object, bool_keys[i]);

      if (output->str[output->len-1] != '{')
        g_string_append_c (output, ',');

      g_string_append_printf (output, " \"%s\" : %s", bool_keys[i], value ? "true" : "false");
    }

  g_string_append (output, " },\n");
}

gboolean
ide_shortcut_bundle_override (IdeShortcutBundle  *self,
                              const char         *shortcut_id,
                              const char         *accelerator,
                              GError            **error)
{
  g_autoptr(GtkShortcutTrigger) trigger = NULL;
  g_autoptr(JsonParser) parser = NULL;
  g_autoptr(GFile) file = NULL;
  g_autoptr(GString) output = NULL;
  g_autofree char *contents = NULL;
  g_autofree char *adjusted = NULL;
  JsonNode *root;
  gboolean found = FALSE;
  gsize len;

  g_return_val_if_fail (IDE_IS_SHORTCUT_BUNDLE (self), FALSE);
  g_return_val_if_fail (self->is_user == TRUE, FALSE);
  g_return_val_if_fail (shortcut_id != NULL, FALSE);

  /* The manager will take care of reloading our keybinding when the file
   * changes so all we need to do here is to load the json file, find the
   * corresponding id as "override" and either remove it (if accelerator NULL)
   * or update it. If it is not found, then we need to add it.
   */

  if (accelerator != NULL)
    trigger = gtk_shortcut_trigger_parse_string (accelerator);

  file = g_file_new_build_filename (g_get_user_config_dir (),
                                    "gnome-builder",
                                    "keybindings.json",
                                    NULL);

  if (!g_file_load_contents (file, NULL, &contents, &len, NULL, NULL))
    contents = g_strdup (""), len = 0;

  g_strstrip (contents);
  len = strlen (contents);

  /* Very brittle, since json-glib doesn't support trailing ","
   * but does allow comments.
   */
  adjusted = g_strdup_printf ("[%s%s{}]",
                              contents,
                              *contents == 0 || g_str_has_suffix (contents, ",") ? "" : ",");

  parser = json_parser_new ();

  if (!json_parser_load_from_data (parser, adjusted, -1, error))
    return FALSE;

  root = json_parser_get_root (parser);
  output = g_string_new (NULL);

  if (JSON_NODE_HOLDS_ARRAY (root))
    {
      JsonArray *root_ar = json_node_get_array (root);
      guint root_ar_len = json_array_get_length (root_ar);

      for (guint i = 0; i < root_ar_len; i++)
        {
          JsonNode *item = json_array_get_element (root_ar, i);
          JsonObject *item_obj;
          const char *str;

          if (!JSON_NODE_HOLDS_OBJECT (item))
            continue;

          item_obj = json_node_get_object (item);

          if (json_object_has_member (item_obj, "override") &&
              (str = json_object_get_string_member (item_obj, "override")) &&
              ide_str_equal0 (str, shortcut_id))
            {
              if (accelerator != NULL)
                g_string_append_printf (output, "{ \"override\" : \"%s\", \"trigger\" : \"%s\" },\n", shortcut_id, accelerator);

              found = TRUE;
            }
          else
            {
              copy_object (item_obj, output);
            }
        }
    }

  if (!found)
    {
      if (accelerator != NULL)
        g_string_append_printf (output, "{ \"override\" : \"%s\", \"trigger\" : \"%s\" },\n", shortcut_id, accelerator);
    }

  return g_file_set_contents (g_file_peek_path (file), output->str, output->len, error);
}
