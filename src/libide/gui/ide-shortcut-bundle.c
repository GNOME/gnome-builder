/* ide-shortcut-bundle.c
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
  GObject    parent_instance;
  GPtrArray *items;
  GError    *error;
};

static TmplScope *imports_scope;

static IdeShortcut *
ide_shortcut_new (const char          *action,
                  GVariant            *args,
                  TmplExpr            *when,
                  GtkPropagationPhase  phase)
{
  IdeShortcut *ret;

  g_assert (action != NULL);
  g_assert (phase == GTK_PHASE_CAPTURE || phase == GTK_PHASE_BUBBLE);

  ret = g_slice_new0 (IdeShortcut);
  ret->action = gtk_named_action_new (action);
  ret->args = args ? g_variant_ref_sink (args) : NULL;
  ret->when = when ? tmpl_expr_ref (when) : NULL;
  ret->phase = phase;

  return ret;
}

static void
ide_shortcut_free (IdeShortcut *shortcut)
{
  g_clear_pointer (&shortcut->when, tmpl_expr_unref);
  g_clear_pointer (&shortcut->args, g_variant_unref);
  g_clear_object (&shortcut->action);
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

  g_clear_pointer (&self->items, g_ptr_array_unref);
  g_clear_error (&self->error);

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
  const char *when_str = NULL;
  const char *args_str = NULL;
  const char *phase_str = NULL;
  const char *command = NULL;
  const char *action = NULL;
  IdeShortcut *state;
  GtkPropagationPhase phase = 0;
  JsonObject *obj;

  g_assert (IDE_IS_SHORTCUT_BUNDLE (self));
  g_assert (node != NULL);
  g_assert (JSON_NODE_HOLDS_OBJECT (node));

  obj = json_node_get_object (node);

  /* TODO: We might want to add title/description so that our internal
   *       keybindings can be displayed to the user from global search
   *       with more than just a command name and/or arguments.
   */

  if (!get_string_member (obj, "trigger", &trigger_str, error) ||
      !get_string_member (obj, "when", &when_str, error) ||
      !get_string_member (obj, "args", &args_str, error) ||
      !get_string_member (obj, "command", &command, error) ||
      !get_string_member (obj, "action", &action, error) ||
      !get_string_member (obj, "phase", &phase_str, error))
    return FALSE;

  if (!(trigger = gtk_shortcut_trigger_parse_string (trigger_str)))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_DATA,
                   "Failed to parse shortcut trigger: \"%s\"",
                   trigger_str);
      return FALSE;
    }

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

  state = ide_shortcut_new (action, args, when, phase);
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
                       "Somthing other than an object found within array");
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
