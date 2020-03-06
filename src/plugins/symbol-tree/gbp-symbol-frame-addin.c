/* gbp-symbol-frame-addin.c
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

#define G_LOG_DOMAIN "gbp-symbol-frame-addin"

#include "config.h"

#include <libide-editor.h>
#include <glib/gi18n.h>

#include "gbp-symbol-frame-addin.h"
#include "gbp-symbol-menu-button.h"

#define CURSOR_MOVED_DELAY_MSEC 500
#define I_(s) (g_intern_static_string(s))

struct _GbpSymbolFrameAddin {
  GObject              parent_instance;

  GbpSymbolMenuButton *button;
  GCancellable        *cancellable;
  GCancellable        *scope_cancellable;
  DzlSignalGroup      *buffer_signals;
  IdePage             *page;

  guint                cursor_moved_handler;
};

typedef struct
{
  GPtrArray         *resolvers;
  IdeBuffer         *buffer;
  IdeLocation *location;
} SymbolResolverTaskData;

static DzlShortcutEntry symbol_tree_shortcuts[] = {
  { "org.gnome.builder.symbol-tree.search",
    0, NULL,
    N_("Editor shortcuts"),
    N_("Symbols"),
    N_("Search symbols within document") },
};

static void
symbol_resolver_task_data_free (SymbolResolverTaskData *data)
{
  g_assert (data != NULL);
  g_assert (data->resolvers != NULL);
  g_assert (data->buffer != NULL);
  g_assert (IDE_IS_BUFFER (data->buffer));

  g_clear_pointer (&data->resolvers, g_ptr_array_unref);
  g_clear_object (&data->buffer);
  g_clear_object (&data->location);
  g_slice_free (SymbolResolverTaskData, data);
}

static void
gbp_symbol_frame_addin_find_scope_cb (GObject      *object,
                                      GAsyncResult *result,
                                      gpointer      user_data)
{
  IdeSymbolResolver *symbol_resolver = (IdeSymbolResolver *)object;
  GbpSymbolFrameAddin *self;
  g_autoptr(IdeSymbol) symbol = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(IdeTask) task = user_data;
  SymbolResolverTaskData *data;

  g_assert (IDE_IS_SYMBOL_RESOLVER (symbol_resolver));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  symbol = ide_symbol_resolver_find_nearest_scope_finish (symbol_resolver, result, &error);
  g_assert (symbol != NULL || error != NULL);

  self = ide_task_get_source_object (task);
  g_assert (GBP_IS_SYMBOL_FRAME_ADDIN (self));

  data = ide_task_get_task_data (task);
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
                                                    gbp_symbol_frame_addin_find_scope_cb,
                                                    g_steal_pointer (&task));

      return;
    }

  if (error != NULL)
    g_debug ("Failed to find nearest scope: %s", error->message);

  if (self->button != NULL)
    gbp_symbol_menu_button_set_symbol (self->button, symbol);

  /* We don't use this, but we should return a value anyway */
  ide_task_return_boolean (task, TRUE);
}

static gboolean
gbp_symbol_frame_addin_cursor_moved_cb (gpointer user_data)
{
  GbpSymbolFrameAddin *self = user_data;
  IdeBuffer *buffer;

  g_assert (GBP_IS_SYMBOL_FRAME_ADDIN (self));

  g_cancellable_cancel (self->scope_cancellable);
  g_clear_object (&self->scope_cancellable);

  buffer = dzl_signal_group_get_target (self->buffer_signals);

  if (buffer != NULL)
    {
      g_autoptr(GPtrArray) resolvers = NULL;

      resolvers = ide_buffer_get_symbol_resolvers (buffer);
      IDE_PTR_ARRAY_SET_FREE_FUNC (resolvers, g_object_unref);

      if (resolvers->len > 0)
        {
          g_autoptr(IdeTask) task = NULL;
          SymbolResolverTaskData *data;
          IdeSymbolResolver *resolver;

          self->scope_cancellable = g_cancellable_new ();

          task = ide_task_new (self, self->scope_cancellable, NULL, NULL);
          ide_task_set_source_tag (task, gbp_symbol_frame_addin_cursor_moved_cb);
          ide_task_set_priority (task, G_PRIORITY_LOW);

          data = g_slice_new0 (SymbolResolverTaskData);
          data->resolvers = g_steal_pointer (&resolvers);
          data->location = ide_buffer_get_insert_location (buffer);
          data->buffer = g_object_ref (buffer);
          ide_task_set_task_data (task, data, symbol_resolver_task_data_free);

          resolver = g_ptr_array_index (data->resolvers, data->resolvers->len - 1);
          /* Go through symbol resolvers one by one to find nearest scope */
          ide_symbol_resolver_find_nearest_scope_async (resolver,
                                                        data->location,
                                                        self->scope_cancellable,
                                                        gbp_symbol_frame_addin_find_scope_cb,
                                                        g_steal_pointer (&task));
        }
    }

  self->cursor_moved_handler = 0;

  return G_SOURCE_REMOVE;
}

static void
gbp_symbol_frame_addin_cursor_moved (GbpSymbolFrameAddin *self,
                                     const GtkTextIter   *location,
                                     IdeBuffer           *buffer)
{
  IdeSourceView *view;
  GSource *source;
  gint64 ready_time;

  g_assert (GBP_IS_SYMBOL_FRAME_ADDIN (self));
  g_assert (location != NULL);
  g_assert (IDE_IS_BUFFER (buffer));

  if (!IDE_IS_EDITOR_PAGE (self->page))
    return;

  view = ide_editor_page_get_view (IDE_EDITOR_PAGE (self->page));
  if (!gtk_widget_has_focus (GTK_WIDGET (view)))
    return;

  if (self->cursor_moved_handler == 0)
    {
      self->cursor_moved_handler =
        gdk_threads_add_timeout_full (G_PRIORITY_LOW,
                                      CURSOR_MOVED_DELAY_MSEC,
                                      gbp_symbol_frame_addin_cursor_moved_cb,
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
gbp_symbol_frame_addin_get_symbol_tree_cb (GObject      *object,
                                           GAsyncResult *result,
                                           gpointer      user_data)
{
  IdeSymbolResolver *symbol_resolver = (IdeSymbolResolver *)object;
  GbpSymbolFrameAddin *self;
  g_autoptr(IdeSymbolTree) tree = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(IdeTask) task = user_data;
  SymbolResolverTaskData *data;

  g_assert (IDE_IS_TASK (task));
  g_assert (IDE_IS_SYMBOL_RESOLVER (symbol_resolver));
  g_assert (G_IS_ASYNC_RESULT (result));

  tree = ide_symbol_resolver_get_symbol_tree_finish (symbol_resolver, result, &error);

  self = ide_task_get_source_object (task);
  data = ide_task_get_task_data (task);

  g_ptr_array_remove_index (data->resolvers, data->resolvers->len - 1);

  /* Ignore empty trees, in favor of next symbol resovler */
  if (tree != NULL && ide_symbol_tree_get_n_children (tree, NULL) == 0)
    g_clear_object (&tree);

  /* If tree is not fetched and symbol resolvers are left then try those */
  if (tree == NULL && data->resolvers->len > 0)
    {
      g_autoptr(GBytes) content = NULL;
      IdeSymbolResolver *resolver;
      GFile *file;

      file = ide_buffer_get_file (data->buffer);
      resolver = g_ptr_array_index (data->resolvers, data->resolvers->len - 1);
      content = ide_buffer_dup_content (data->buffer);

      ide_symbol_resolver_get_symbol_tree_async (resolver,
                                                 file,
                                                 content,
                                                 self->cancellable,
                                                 gbp_symbol_frame_addin_get_symbol_tree_cb,
                                                 g_steal_pointer (&task));
      return;
    }

  if (error != NULL)
    g_debug ("Failed to get symbol tree: %s", error->message);

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
  ide_task_return_boolean (task, TRUE);
}

static void
gbp_symbol_frame_addin_update_tree (GbpSymbolFrameAddin *self,
                                    IdeBuffer           *buffer)
{
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GPtrArray) resolvers = NULL;
  g_autoptr(GBytes) content = NULL;
  SymbolResolverTaskData *data;
  IdeSymbolResolver *resolver;
  GFile *file;

  g_assert (GBP_IS_SYMBOL_FRAME_ADDIN (self));
  g_assert (IDE_IS_BUFFER (buffer));

  /* Cancel any in-flight work */
  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);

  resolvers = ide_buffer_get_symbol_resolvers (buffer);
  IDE_PTR_ARRAY_SET_FREE_FUNC (resolvers, g_object_unref);

  if (resolvers->len == 0)
    {
      gtk_widget_hide (GTK_WIDGET (self->button));
      return;
    }

  gtk_widget_show (GTK_WIDGET (self->button));

  file = ide_buffer_get_file (buffer);
  g_assert (G_IS_FILE (file));

  content = ide_buffer_dup_content (buffer);

  self->cancellable = g_cancellable_new ();

  task = ide_task_new (self, self->cancellable, NULL, NULL);
  ide_task_set_source_tag (task, gbp_symbol_frame_addin_update_tree);
  ide_task_set_priority (task, G_PRIORITY_LOW);

  data = g_slice_new0 (SymbolResolverTaskData);
  data->resolvers = g_steal_pointer (&resolvers);
  data->buffer = g_object_ref (buffer);
  ide_task_set_task_data (task, data, symbol_resolver_task_data_free);

  g_assert (data->resolvers->len > 0);

  resolver = g_ptr_array_index (data->resolvers, data->resolvers->len - 1);
  ide_symbol_resolver_get_symbol_tree_async (resolver,
                                             file,
                                             content,
                                             self->cancellable,
                                             gbp_symbol_frame_addin_get_symbol_tree_cb,
                                             g_steal_pointer (&task));
}

static void
gbp_symbol_frame_addin_change_settled (GbpSymbolFrameAddin *self,
                                       IdeBuffer           *buffer)
{
  g_assert (GBP_IS_SYMBOL_FRAME_ADDIN (self));
  g_assert (IDE_IS_BUFFER (buffer));

  /* Ignore this request unless the button is active */
  if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->button)))
    return;

  gbp_symbol_frame_addin_update_tree (self, buffer);
}

static void
gbp_symbol_frame_addin_button_toggled (GbpSymbolFrameAddin *self,
                                       GtkMenuButton       *button)
{
  IdeBuffer *buffer;

  g_assert (GBP_IS_SYMBOL_FRAME_ADDIN (self));
  g_assert (GTK_IS_MENU_BUTTON (button));

  buffer = dzl_signal_group_get_target (self->buffer_signals);
  g_assert (!buffer || IDE_IS_BUFFER (buffer));

  if (buffer != NULL && gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)))
    gbp_symbol_frame_addin_update_tree (self, buffer);
}

static void
gbp_symbol_frame_addin_notify_has_symbol_resolvers (GbpSymbolFrameAddin *self,
                                                    GParamSpec          *pspec,
                                                    IdeBuffer           *buffer)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_SYMBOL_FRAME_ADDIN (self));
  g_assert (IDE_IS_BUFFER (buffer));

  gtk_widget_set_visible (GTK_WIDGET (self->button),
                          ide_buffer_has_symbol_resolvers (buffer));
  gbp_symbol_frame_addin_update_tree (self, buffer);
}

static void
gbp_symbol_frame_addin_bind (GbpSymbolFrameAddin *self,
                             IdeBuffer           *buffer,
                             DzlSignalGroup      *buffer_signals)
{
  g_autoptr(GPtrArray) resolvers = NULL;

  g_assert (GBP_IS_SYMBOL_FRAME_ADDIN (self));
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (DZL_IS_SIGNAL_GROUP (buffer_signals));

  self->cancellable = g_cancellable_new ();

  gbp_symbol_menu_button_set_symbol (self->button, NULL);
  gbp_symbol_frame_addin_notify_has_symbol_resolvers (self, NULL, buffer);
}

static void
gbp_symbol_frame_addin_unbind (GbpSymbolFrameAddin *self,
                               DzlSignalGroup      *buffer_signals)
{
  g_assert (GBP_IS_SYMBOL_FRAME_ADDIN (self));
  g_assert (DZL_IS_SIGNAL_GROUP (buffer_signals));

  dzl_clear_source (&self->cursor_moved_handler);

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);

  g_cancellable_cancel (self->scope_cancellable);
  g_clear_object (&self->scope_cancellable);

  gtk_widget_hide (GTK_WIDGET (self->button));
}

static void
search_action_cb (GSimpleAction *action,
                  GVariant      *param,
                  gpointer       user_data)
{
  GbpSymbolFrameAddin *self = user_data;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (GBP_IS_SYMBOL_FRAME_ADDIN (self));

  if (gtk_widget_get_visible (GTK_WIDGET (self->button)))
    gtk_widget_activate (GTK_WIDGET (self->button));
}

static void
gbp_symbol_frame_addin_load (IdeFrameAddin *addin,
                             IdeFrame      *stack)
{
  GbpSymbolFrameAddin *self = (GbpSymbolFrameAddin *)addin;
  g_autoptr(GSimpleActionGroup) actions = NULL;
  DzlShortcutController *controller;
  GtkWidget *header;
  static const GActionEntry entries[] = {
    { "search", search_action_cb },
  };

  g_assert (GBP_IS_SYMBOL_FRAME_ADDIN (self));
  g_assert (IDE_IS_FRAME (stack));

  controller = dzl_shortcut_controller_find (GTK_WIDGET (stack));

  dzl_shortcut_controller_add_command_action (controller,
                                              I_("org.gnome.builder.symbol-tree.search"),
                                              "<Primary><Shift>k",
                                              DZL_SHORTCUT_PHASE_BUBBLE,
                                              I_("symbol-tree.search"));

  actions = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (actions),
                                   entries,
                                   G_N_ELEMENTS (entries),
                                   self);
  gtk_widget_insert_action_group (GTK_WIDGET (stack),
                                  "symbol-tree",
                                  G_ACTION_GROUP (actions));

  /* Add our menu button to the header */
  header = ide_frame_get_titlebar (stack);
  self->button = g_object_new (GBP_TYPE_SYMBOL_MENU_BUTTON, NULL);
  g_signal_connect (self->button,
                    "destroy",
                    G_CALLBACK (gtk_widget_destroyed),
                    &self->button);
  g_signal_connect_swapped (self->button,
                            "toggled",
                            G_CALLBACK (gbp_symbol_frame_addin_button_toggled),
                            self);
  ide_frame_header_add_custom_title (IDE_FRAME_HEADER (header),
                                            GTK_WIDGET (self->button),
                                            100);

  /* Setup our signals to the buffer */
  self->buffer_signals = dzl_signal_group_new (IDE_TYPE_BUFFER);

  g_signal_connect_swapped (self->buffer_signals,
                            "bind",
                            G_CALLBACK (gbp_symbol_frame_addin_bind),
                            self);

  g_signal_connect_swapped (self->buffer_signals,
                            "unbind",
                            G_CALLBACK (gbp_symbol_frame_addin_unbind),
                            self);

  dzl_signal_group_connect_swapped (self->buffer_signals,
                                    "cursor-moved",
                                    G_CALLBACK (gbp_symbol_frame_addin_cursor_moved),
                                    self);

  dzl_signal_group_connect_swapped (self->buffer_signals,
                                    "change-settled",
                                    G_CALLBACK (gbp_symbol_frame_addin_change_settled),
                                    self);
  dzl_signal_group_connect_swapped (self->buffer_signals,
                                    "notify::has-symbol-resolvers",
                                    G_CALLBACK (gbp_symbol_frame_addin_notify_has_symbol_resolvers),
                                    self);
}

static void
gbp_symbol_frame_addin_unload (IdeFrameAddin *addin,
                               IdeFrame      *stack)
{
  GbpSymbolFrameAddin *self = (GbpSymbolFrameAddin *)addin;

  g_assert (GBP_IS_SYMBOL_FRAME_ADDIN (self));
  g_assert (IDE_IS_FRAME (stack));

  self->page = NULL;

  gtk_widget_insert_action_group (GTK_WIDGET (stack), "symbol-tree", NULL);

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);
  g_clear_object (&self->buffer_signals);

  if (self->button != NULL)
    gtk_widget_destroy (GTK_WIDGET (self->button));
}

static void
gbp_symbol_frame_addin_set_page (IdeFrameAddin *addin,
                                 IdePage       *page)
{
  GbpSymbolFrameAddin *self = (GbpSymbolFrameAddin *)addin;
  IdeBuffer *buffer = NULL;

  g_assert (GBP_IS_SYMBOL_FRAME_ADDIN (self));
  g_assert (!page || IDE_IS_PAGE (page));

  self->page = page;

  /* First clear any old symbol tree */
  gbp_symbol_menu_button_set_symbol_tree (self->button, NULL);

  if (IDE_IS_EDITOR_PAGE (page))
    buffer = ide_editor_page_get_buffer (IDE_EDITOR_PAGE (page));

  dzl_signal_group_set_target (self->buffer_signals, buffer);
}

static void
frame_addin_iface_init (IdeFrameAddinInterface *iface)
{
  iface->load = gbp_symbol_frame_addin_load;
  iface->unload = gbp_symbol_frame_addin_unload;
  iface->set_page = gbp_symbol_frame_addin_set_page;
}

G_DEFINE_TYPE_WITH_CODE (GbpSymbolFrameAddin,
                         gbp_symbol_frame_addin,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_FRAME_ADDIN,
                                                frame_addin_iface_init))

static void
gbp_symbol_frame_addin_class_init (GbpSymbolFrameAddinClass *klass)
{
}

static void
gbp_symbol_frame_addin_init (GbpSymbolFrameAddin *self)
{
  dzl_shortcut_manager_add_shortcut_entries (NULL,
                                             symbol_tree_shortcuts,
                                             G_N_ELEMENTS (symbol_tree_shortcuts),
                                             GETTEXT_PACKAGE);
}
