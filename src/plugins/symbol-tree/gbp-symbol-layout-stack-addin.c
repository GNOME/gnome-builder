/* gbp-symbol-layout-stack-addin.c
 *
 * Copyright © 2017 Christian Hergert <chergert@redhat.com>
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
 */

#define G_LOG_DOMAIN "gbp-symbol-layout-stack-addin"

#include <glib/gi18n.h>

#include "gbp-symbol-layout-stack-addin.h"
#include "gbp-symbol-menu-button.h"

#define CURSOR_MOVED_DELAY_MSEC 500

struct _GbpSymbolLayoutStackAddin {
  GObject              parent_instance;

  GbpSymbolMenuButton *button;
  GCancellable        *cancellable;
  GCancellable        *scope_cancellable;
  DzlSignalGroup      *buffer_signals;

  guint                cursor_moved_handler;

  guint                resolvers_loaded : 1;
};

typedef struct
{
  GPtrArray         *resolvers;
  IdeBuffer         *buffer;
  IdeSourceLocation *location;
} SymbolResolverTaskData;

static void
symbol_resolver_task_data_free (SymbolResolverTaskData *data)
{
  g_assert (data != NULL);
  g_assert (data->resolvers != NULL);
  g_assert (data->buffer != NULL);
  g_assert (IDE_IS_BUFFER (data->buffer));

  g_clear_pointer (&data->resolvers, g_ptr_array_unref);
  g_clear_object (&data->buffer);
  g_clear_pointer (&data->location, ide_source_location_unref);
  g_slice_free (SymbolResolverTaskData, data);
}

static void
get_extension (IdeExtensionSetAdapter *set,
               PeasPluginInfo         *plugin_info,
               PeasExtension          *extension,
               gpointer                user_data)
{
  SymbolResolverTaskData *data = user_data;
  IdeSymbolResolver *resolver = (IdeSymbolResolver *)extension;

  g_assert (data != NULL);
  g_assert (data->resolvers != NULL);
  g_assert (IDE_IS_SYMBOL_RESOLVER (resolver));

  g_ptr_array_add (data->resolvers, g_object_ref (resolver));
}

static void
gbp_symbol_layout_stack_addin_find_scope_cb (GObject      *object,
                                             GAsyncResult *result,
                                             gpointer      user_data)
{
  IdeSymbolResolver *symbol_resolver = (IdeSymbolResolver *)object;
  GbpSymbolLayoutStackAddin *self;
  g_autoptr(IdeSymbol) symbol = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GTask) task = user_data;
  SymbolResolverTaskData *data;

  g_assert (IDE_IS_SYMBOL_RESOLVER (symbol_resolver));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  symbol = ide_symbol_resolver_find_nearest_scope_finish (symbol_resolver, result, &error);
  g_assert (symbol != NULL || error != NULL);

  self = g_task_get_source_object (task);
  g_assert (GBP_IS_SYMBOL_LAYOUT_STACK_ADDIN (self));

  data = g_task_get_task_data (task);
  g_assert (data != NULL);
  g_assert (IDE_IS_BUFFER (data->buffer));
  g_assert (data->resolvers != NULL);
  g_assert (data->resolvers->len > 0);

  g_ptr_array_remove_index (data->resolvers, data->resolvers->len - 1);

  /* If symbol is not found and symbol resolvers are left try those */
  if (symbol == NULL && data->resolvers->len > 0)
    {
      IdeSymbolResolver *resolver;

      resolver = g_ptr_array_index (data->resolvers, data->resolvers->len - 1);
      ide_symbol_resolver_find_nearest_scope_async (resolver,
                                                    data->location,
                                                    self->scope_cancellable,
                                                    gbp_symbol_layout_stack_addin_find_scope_cb,
                                                    g_steal_pointer (&task));

      return;
    }

  if (error != NULL &&
      !(g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED) ||
        g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND) ||
        g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED)))
    g_warning ("Failed to find nearest scope: %s", error->message);

  if (self->button != NULL)
    gbp_symbol_menu_button_set_symbol (self->button, symbol);

  /* We don't use this, but we should return a value anyway */
  g_task_return_boolean (task, TRUE);
}

static gboolean
gbp_symbol_layout_stack_addin_cursor_moved_cb (gpointer user_data)
{
  GbpSymbolLayoutStackAddin *self = user_data;
  IdeBuffer *buffer;

  g_assert (GBP_IS_SYMBOL_LAYOUT_STACK_ADDIN (self));

  g_cancellable_cancel (self->scope_cancellable);
  g_clear_object (&self->scope_cancellable);

  buffer = dzl_signal_group_get_target (self->buffer_signals);

  if (buffer != NULL)
    {
      IdeExtensionSetAdapter *adapter;

      adapter = ide_buffer_get_symbol_resolvers (buffer);

      if (ide_extension_set_adapter_get_n_extensions (adapter))
        {
          g_autoptr(GTask) task = NULL;
          SymbolResolverTaskData *data;
          IdeSymbolResolver *resolver;

          self->scope_cancellable = g_cancellable_new ();

          task = g_task_new (self, self->scope_cancellable, NULL, NULL);
          g_task_set_source_tag (task, gbp_symbol_layout_stack_addin_cursor_moved_cb);
          g_task_set_priority (task, G_PRIORITY_LOW);

          data = g_slice_new0 (SymbolResolverTaskData);
          data->resolvers = g_ptr_array_new_with_free_func (g_object_unref);
          data->location = ide_buffer_get_insert_location (buffer);
          data->buffer = g_object_ref (buffer);
          g_task_set_task_data (task, data, (GDestroyNotify)symbol_resolver_task_data_free);

          ide_extension_set_adapter_foreach (adapter, get_extension, data);
          g_assert (data->resolvers->len > 0);

          resolver = g_ptr_array_index (data->resolvers, data->resolvers->len - 1);
          /* Go through symbol resolvers one by one to find nearest scope */
          ide_symbol_resolver_find_nearest_scope_async (resolver,
                                                        data->location,
                                                        self->scope_cancellable,
                                                        gbp_symbol_layout_stack_addin_find_scope_cb,
                                                        g_steal_pointer (&task));
        }
    }

  self->cursor_moved_handler = 0;

  return G_SOURCE_REMOVE;
}

static void
gbp_symbol_layout_stack_addin_cursor_moved (GbpSymbolLayoutStackAddin *self,
                                            const GtkTextIter         *location,
                                            IdeBuffer                 *buffer)
{
  GSource *source;
  gint64 ready_time;

  g_assert (GBP_IS_SYMBOL_LAYOUT_STACK_ADDIN (self));
  g_assert (location != NULL);
  g_assert (IDE_IS_BUFFER (buffer));

  if (self->cursor_moved_handler == 0)
    {
      self->cursor_moved_handler =
        gdk_threads_add_timeout_full (G_PRIORITY_LOW,
                                      CURSOR_MOVED_DELAY_MSEC,
                                      gbp_symbol_layout_stack_addin_cursor_moved_cb,
                                      g_object_ref (self),
                                      g_object_unref);
      return;
    }

  /* Try to reuse our existing GSource if we can */
  ready_time = g_get_monotonic_time () + (CURSOR_MOVED_DELAY_MSEC * 1000);
  source = g_main_context_find_source_by_id (NULL, self->cursor_moved_handler);
  g_source_set_ready_time (source, ready_time);
}

static void
gbp_symbol_layout_stack_addin_get_symbol_tree_cb (GObject      *object,
                                                  GAsyncResult *result,
                                                  gpointer      user_data)
{
  IdeSymbolResolver *symbol_resolver = (IdeSymbolResolver *)object;
  GbpSymbolLayoutStackAddin *self;
  g_autoptr(IdeSymbolTree) tree = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GTask) task = user_data;
  SymbolResolverTaskData *data;

  g_assert (G_IS_TASK (task));
  g_assert (IDE_IS_SYMBOL_RESOLVER (symbol_resolver));
  g_assert (G_IS_ASYNC_RESULT (result));

  tree = ide_symbol_resolver_get_symbol_tree_finish (symbol_resolver, result, &error);

  self = g_task_get_source_object (task);
  data = g_task_get_task_data (task);

  g_ptr_array_remove_index (data->resolvers, data->resolvers->len - 1);

  /* Ignore empty trees, in favor of next symbol resovler */
  if (tree != NULL && ide_symbol_tree_get_n_children (tree, NULL) == 0)
    g_clear_object (&tree);

  /* If tree is not fetched and symbol resolvers are left then try those */
  if (tree == NULL && data->resolvers->len > 0)
    {
      GFile *file;
      IdeSymbolResolver *resolver;

      file = ide_file_get_file (ide_buffer_get_file (data->buffer));
      resolver = g_ptr_array_index (data->resolvers, data->resolvers->len - 1);

      ide_symbol_resolver_get_symbol_tree_async (resolver,
                                                 file,
                                                 data->buffer,
                                                 self->cancellable,
                                                 gbp_symbol_layout_stack_addin_get_symbol_tree_cb,
                                                 g_steal_pointer (&task));
      return;
    }

  if (error != NULL &&
      !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED) &&
      !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED))
    g_warning ("Failed to get symbol tree: %s", error->message);

  /* If we were destroyed, short-circuit */
  if (self->button != NULL)
    {
      /* Only override if we got a new value (this helps with situations
       * where the parse tree breaks intermittently.
       */
      if (tree != NULL)
        gbp_symbol_menu_button_set_symbol_tree (self->button, tree);
    }

  /* We don't use this, but we should return a value anyway */
  g_task_return_boolean (task, TRUE);
}

static void
gbp_symbol_layout_stack_addin_update_tree (GbpSymbolLayoutStackAddin *self,
                                           IdeBuffer                 *buffer)
{
  IdeExtensionSetAdapter *adapter;
  IdeFile *file;
  g_autoptr(GTask) task = NULL;
  SymbolResolverTaskData *data;
  IdeSymbolResolver *resolver;

  g_assert (GBP_IS_SYMBOL_LAYOUT_STACK_ADDIN (self));
  g_assert (IDE_IS_BUFFER (buffer));

  /* Cancel any in-flight work */
  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);

  adapter = ide_buffer_get_symbol_resolvers (buffer);

  gtk_widget_set_visible (GTK_WIDGET (self->button),
                          ide_extension_set_adapter_get_n_extensions (adapter));

  if (!ide_extension_set_adapter_get_n_extensions (adapter))
    return;

  file = ide_buffer_get_file (buffer);
  g_assert (IDE_IS_FILE (file));

  self->cancellable = g_cancellable_new ();

  task = g_task_new (self, self->cancellable, NULL, NULL);
  g_task_set_source_tag (task, gbp_symbol_layout_stack_addin_update_tree);
  g_task_set_priority (task, G_PRIORITY_LOW);

  data = g_slice_new0 (SymbolResolverTaskData);
  data->resolvers = g_ptr_array_new_with_free_func (g_object_unref);
  data->buffer = g_object_ref (buffer);
  g_task_set_task_data (task, data, (GDestroyNotify)symbol_resolver_task_data_free);

  ide_extension_set_adapter_foreach (adapter, get_extension, data);
  g_assert (data->resolvers->len > 0);

  resolver = g_ptr_array_index (data->resolvers, data->resolvers->len - 1);
  ide_symbol_resolver_get_symbol_tree_async (resolver,
                                             ide_file_get_file (file),
                                             data->buffer,
                                             self->cancellable,
                                             gbp_symbol_layout_stack_addin_get_symbol_tree_cb,
                                             g_steal_pointer (&task));
}

static void
gbp_symbol_layout_stack_addin_change_settled (GbpSymbolLayoutStackAddin *self,
                                              IdeBuffer                 *buffer)
{
  g_assert (GBP_IS_SYMBOL_LAYOUT_STACK_ADDIN (self));
  g_assert (IDE_IS_BUFFER (buffer));

  /* Ignore this request unless the button is active */
  if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->button)))
    return;

  gbp_symbol_layout_stack_addin_update_tree (self, buffer);
}

static void
gbp_symbol_layout_stack_addin_button_toggled (GbpSymbolLayoutStackAddin *self,
                                              GtkMenuButton             *button)
{
  IdeBuffer *buffer;

  g_assert (GBP_IS_SYMBOL_LAYOUT_STACK_ADDIN (self));
  g_assert (GTK_IS_MENU_BUTTON (button));

  buffer = dzl_signal_group_get_target (self->buffer_signals);
  g_assert (!buffer || IDE_IS_BUFFER (buffer));

  if (buffer != NULL && gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)))
    gbp_symbol_layout_stack_addin_update_tree (self, buffer);
}

static void
gbp_symbol_layout_stack_addin_bind (GbpSymbolLayoutStackAddin *self,
                                    IdeBuffer                 *buffer,
                                    DzlSignalGroup            *buffer_signals)
{
  IdeExtensionSetAdapter *adapter;
  guint n_extensions;

  g_assert (GBP_IS_SYMBOL_LAYOUT_STACK_ADDIN (self));
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (DZL_IS_SIGNAL_GROUP (buffer_signals));

  self->cancellable = g_cancellable_new ();

  gbp_symbol_menu_button_set_symbol (self->button, NULL);

  if (self->resolvers_loaded)
    return;

  adapter = ide_buffer_get_symbol_resolvers (buffer);
  n_extensions = ide_extension_set_adapter_get_n_extensions (adapter);

  if (n_extensions)
    self->resolvers_loaded = TRUE;

  gtk_widget_set_visible (GTK_WIDGET (self->button), n_extensions);
  gbp_symbol_layout_stack_addin_update_tree (self, buffer);
}

static void
gbp_symbol_layout_stack_addin_unbind (GbpSymbolLayoutStackAddin *self,
                                      DzlSignalGroup            *buffer_signals)
{
  g_assert (GBP_IS_SYMBOL_LAYOUT_STACK_ADDIN (self));
  g_assert (DZL_IS_SIGNAL_GROUP (buffer_signals));

  dzl_clear_source (&self->cursor_moved_handler);

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);

  g_cancellable_cancel (self->scope_cancellable);
  g_clear_object (&self->scope_cancellable);

  gtk_widget_hide (GTK_WIDGET (self->button));
  self->resolvers_loaded = FALSE;
}

static void
gbp_symbol_layout_stack_addin_symbol_resolvers_loaded (GbpSymbolLayoutStackAddin *self,
                                                       IdeBuffer                 *buffer)
{
  IdeExtensionSetAdapter *adapter;
  guint n_extensions;

  g_assert (GBP_IS_SYMBOL_LAYOUT_STACK_ADDIN (self));
  g_assert (IDE_IS_BUFFER (buffer));

  if (self->resolvers_loaded)
    return;

  adapter = ide_buffer_get_symbol_resolvers (buffer);
  n_extensions = ide_extension_set_adapter_get_n_extensions (adapter);

  gtk_widget_set_visible (GTK_WIDGET (self->button), n_extensions);
  self->resolvers_loaded = TRUE;

  gbp_symbol_layout_stack_addin_update_tree (self, buffer);
}

static void
gbp_symbol_layout_stack_addin_load (IdeLayoutStackAddin *addin,
                                    IdeLayoutStack      *stack)
{
  GbpSymbolLayoutStackAddin *self = (GbpSymbolLayoutStackAddin *)addin;
  GtkWidget *header;

  g_assert (GBP_IS_SYMBOL_LAYOUT_STACK_ADDIN (self));
  g_assert (IDE_IS_LAYOUT_STACK (stack));

  /* Add our menu button to the header */
  header = ide_layout_stack_get_titlebar (stack);
  self->button = g_object_new (GBP_TYPE_SYMBOL_MENU_BUTTON, NULL);
  g_signal_connect (self->button,
                    "destroy",
                    G_CALLBACK (gtk_widget_destroyed),
                    &self->button);
  g_signal_connect_swapped (self->button,
                            "toggled",
                            G_CALLBACK (gbp_symbol_layout_stack_addin_button_toggled),
                            self);
  ide_layout_stack_header_add_custom_title (IDE_LAYOUT_STACK_HEADER (header),
                                            GTK_WIDGET (self->button),
                                            100);

  /* Setup our signals to the buffer */
  self->buffer_signals = dzl_signal_group_new (IDE_TYPE_BUFFER);

  g_signal_connect_swapped (self->buffer_signals,
                            "bind",
                            G_CALLBACK (gbp_symbol_layout_stack_addin_bind),
                            self);

  g_signal_connect_swapped (self->buffer_signals,
                            "unbind",
                            G_CALLBACK (gbp_symbol_layout_stack_addin_unbind),
                            self);

  dzl_signal_group_connect_swapped (self->buffer_signals,
                                    "cursor-moved",
                                    G_CALLBACK (gbp_symbol_layout_stack_addin_cursor_moved),
                                    self);

  dzl_signal_group_connect_swapped (self->buffer_signals,
                                    "change-settled",
                                    G_CALLBACK (gbp_symbol_layout_stack_addin_change_settled),
                                    self);
  dzl_signal_group_connect_swapped (self->buffer_signals,
                                    "symbol-resolvers-loaded",
                                    G_CALLBACK (gbp_symbol_layout_stack_addin_symbol_resolvers_loaded),
                                    self);
}

static void
gbp_symbol_layout_stack_addin_unload (IdeLayoutStackAddin *addin,
                                      IdeLayoutStack      *stack)
{
  GbpSymbolLayoutStackAddin *self = (GbpSymbolLayoutStackAddin *)addin;

  g_assert (GBP_IS_SYMBOL_LAYOUT_STACK_ADDIN (self));
  g_assert (IDE_IS_LAYOUT_STACK (stack));

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);
  g_clear_object (&self->buffer_signals);

  if (self->button != NULL)
    gtk_widget_destroy (GTK_WIDGET (self->button));
}

static void
gbp_symbol_layout_stack_addin_set_view (IdeLayoutStackAddin *addin,
                                        IdeLayoutView       *view)
{
  GbpSymbolLayoutStackAddin *self = (GbpSymbolLayoutStackAddin *)addin;
  IdeBuffer *buffer = NULL;

  g_assert (GBP_IS_SYMBOL_LAYOUT_STACK_ADDIN (self));
  g_assert (!view || IDE_IS_LAYOUT_VIEW (view));

  /* First clear any old symbol tree */
  gbp_symbol_menu_button_set_symbol_tree (self->button, NULL);

  if (IDE_IS_EDITOR_VIEW (view))
    buffer = ide_editor_view_get_buffer (IDE_EDITOR_VIEW (view));

  dzl_signal_group_set_target (self->buffer_signals, buffer);
}

static void
layout_stack_addin_iface_init (IdeLayoutStackAddinInterface *iface)
{
  iface->load = gbp_symbol_layout_stack_addin_load;
  iface->unload = gbp_symbol_layout_stack_addin_unload;
  iface->set_view = gbp_symbol_layout_stack_addin_set_view;
}

G_DEFINE_TYPE_WITH_CODE (GbpSymbolLayoutStackAddin,
                         gbp_symbol_layout_stack_addin,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_LAYOUT_STACK_ADDIN,
                                                layout_stack_addin_iface_init))

static void
gbp_symbol_layout_stack_addin_class_init (GbpSymbolLayoutStackAddinClass *klass)
{
}

static void
gbp_symbol_layout_stack_addin_init (GbpSymbolLayoutStackAddin *self)
{
}
