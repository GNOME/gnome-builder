/* ide-session.c
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

#define G_LOG_DOMAIN "ide-session"

#include "config.h"

#include <libpeas/peas.h>
#include <libide-plugins.h>
#include <libide-gui.h>
#include <libide-threading.h>

#include "ide-session-addin.h"
#include "ide-session-private.h"

#include "ide-gui-private.h"

struct _IdeSession
{
  IdeObject               parent_instance;
  GPtrArray              *addins;
};

typedef struct
{
  GPtrArray      *addins;
  GVariantBuilder pages_state;
  guint           active;
  IdeGrid        *grid;
} Save;

typedef struct
{
  GPtrArray *addins;
  GVariant  *state;
  IdeGrid   *grid;
  GArray    *pages;
  guint      active;
} Restore;

typedef struct
{
  guint            column;
  guint            row;
  guint            depth;
  IdeSessionAddin *addin;
  GVariant        *state;
  IdePage         *restored_page;
} RestoreItem;

G_DEFINE_FINAL_TYPE (IdeSession, ide_session, IDE_TYPE_OBJECT)

static void
restore_free (Restore *r)
{
  g_assert (r != NULL);
  g_assert (r->active == 0);

  g_clear_pointer (&r->state, g_variant_unref);
  g_clear_pointer (&r->pages, g_array_unref);

  g_slice_free (Restore, r);
}

static void
save_free (Save *s)
{
  g_assert (s != NULL);
  g_assert (s->active == 0);

  g_slice_free (Save, s);
}

static void
restore_item_clear (RestoreItem *item)
{
  g_assert (item != NULL);

  g_clear_pointer (&item->state, g_variant_unref);
}

static gint
compare_restore_items (gconstpointer a,
                       gconstpointer b)
{
  const RestoreItem *item_a = a;
  const RestoreItem *item_b = b;
  gint ret;

  if (!(ret = item_a->column - item_b->column))
    {
      if (!(ret = item_a->row - item_b->row))
        ret = item_a->depth - item_b->depth;
    }

  return ret;
}

static void
collect_addins_cb (IdeExtensionSetAdapter *set,
                   PeasPluginInfo         *plugin_info,
                   PeasExtension          *exten,
                   gpointer                user_data)
{
  GPtrArray *ar = user_data;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_SESSION_ADDIN (exten));
  g_assert (ar != NULL);

  g_ptr_array_add (ar, g_object_ref (exten));
}

static IdeSessionAddin *
find_suitable_addin_for_page (IdePage   *page,
                              GPtrArray *addins)
{
  for (guint i = 0; i < addins->len; i++)
    {
      IdeSessionAddin *addin = g_ptr_array_index (addins, i);
      if (ide_session_addin_can_save_page (addin, page))
        return addin;
    }
  return NULL;
}

static void
on_session_autosaved_cb (GObject      *object,
                         GAsyncResult *result,
                         gpointer      user_data)
{
  IdeSession *session = (IdeSession *)object;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_SESSION (session));
  g_assert (G_IS_ASYNC_RESULT (result));

  if (!ide_session_save_finish (session, result, &error))
    g_warning ("Couldn't autosave session: %s", error->message);
}

typedef struct {
  IdeSession *session;
  IdeGrid    *grid;
  guint       session_autosave_source;
} AutosaveGrid;

static void
autosave_grid_free (gpointer data,
                    GClosure *closure)
{
  AutosaveGrid *self = (AutosaveGrid *)data;

  if (self->session_autosave_source)
    {
      g_source_remove (self->session_autosave_source);
      self->session_autosave_source = 0;
    }
  g_slice_free (AutosaveGrid, self);
}

static gboolean
on_session_autosave_timeout_cb (gpointer user_data)
{
  AutosaveGrid *autosave_grid = (AutosaveGrid *)user_data;

  g_assert (IDE_IS_SESSION (autosave_grid->session));
  g_assert (IDE_IS_GRID (autosave_grid->grid));

  ide_session_save_async (autosave_grid->session,
                          autosave_grid->grid,
                          NULL,
                          on_session_autosaved_cb,
                          NULL);

  autosave_grid->session_autosave_source = 0;

  return G_SOURCE_REMOVE;
}

static void
schedule_session_autosave_timeout (AutosaveGrid *autosave_grid)
{
  if (!autosave_grid->session_autosave_source)
    {
      /* We don't want to be saving the state on each (small) change, so introduce a small
       * timeout so changes are grouped when saving.
       */
      autosave_grid->session_autosave_source =
        g_timeout_add_seconds (30,
                               on_session_autosave_timeout_cb,
                               autosave_grid);
    }
}

static void
on_autosave_property_changed_cb (GObject    *gobject,
                                 GParamSpec *pspec,
                                 gpointer    user_data)
{
  schedule_session_autosave_timeout ((AutosaveGrid *)user_data);
}

static void
watch_pages_session_autosave (AutosaveGrid *autosave_grid,
                              guint         start_pos,
                              guint         end_pos)
{
  GListModel *list = (GListModel *)autosave_grid->grid;
  IdeSession *session = (IdeSession *)autosave_grid->session;

  g_assert (IDE_IS_SESSION (session));
  g_assert (G_IS_LIST_MODEL (list));
  g_assert (g_type_is_a (g_list_model_get_item_type (list), IDE_TYPE_PAGE));
  g_assert (start_pos <= end_pos);

  for (guint i = start_pos; i < end_pos; i++)
    {
      IdePage *page = IDE_PAGE (g_list_model_get_object (list, i));
      IdeSessionAddin *addin;
      g_auto(GStrv) props = NULL;

      if ((addin = find_suitable_addin_for_page (page, session->addins)) &&
          (props = ide_session_addin_get_autosave_properties (addin)))
        {
          for (guint j = 0; props[j] != NULL; j++)
            {
              char detailed_signal[256];
              g_snprintf (detailed_signal, sizeof detailed_signal, "notify::%s", props[j]);

              g_signal_connect (page, detailed_signal, G_CALLBACK (on_autosave_property_changed_cb), autosave_grid);
            }
        }
    }
}

static void
on_grid_items_changed_cb (GListModel *list,
                          guint       position,
                          guint       removed,
                          guint       added,
                          gpointer    user_data)
{
  AutosaveGrid *autosave_grid = (AutosaveGrid *)user_data;

  g_assert (G_IS_LIST_MODEL (list));
  g_assert (g_type_is_a (g_list_model_get_item_type (list), IDE_TYPE_PAGE));

  /* We've nothing to do when no page were added here as signals are
   * automatically disconnected, so avoid extra work by stopping here early.
   */
  if (added > 0)
    watch_pages_session_autosave (autosave_grid, position, position + added);

  /* Handles autosaving both when closing/opening a page and when moving a page in the grid. */
  schedule_session_autosave_timeout (autosave_grid);
}

static void
ide_session_destroy (IdeObject *object)
{
  IdeSession *self = (IdeSession *)object;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_SESSION (self));

  g_clear_pointer (&self->addins, g_ptr_array_unref);

  IDE_OBJECT_CLASS (ide_session_parent_class)->destroy (object);

  IDE_EXIT;
}

static void
ide_session_parent_set (IdeObject *object,
                        IdeObject *parent)
{
  IdeSession *self = (IdeSession *)object;
  g_autoptr(IdeExtensionSetAdapter) extension_set = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_SESSION (self));
  g_assert (!parent || IDE_IS_OBJECT (parent));

  if (parent == NULL)
    return;

  extension_set = ide_extension_set_adapter_new (IDE_OBJECT (self),
                                                 peas_engine_get_default (),
                                                 IDE_TYPE_SESSION_ADDIN,
                                                 NULL, NULL);

  self->addins = g_ptr_array_new_with_free_func (g_object_unref);
  ide_extension_set_adapter_foreach (extension_set, collect_addins_cb, self->addins);
}

static void
ide_session_class_init (IdeSessionClass *klass)
{
  IdeObjectClass *i_object_class = IDE_OBJECT_CLASS (klass);

  i_object_class->destroy = ide_session_destroy;
  i_object_class->parent_set = ide_session_parent_set;
}

static void
ide_session_init (IdeSession *self)
{
}

static void
restore_pages_to_grid (GArray  *r_items,
                       IdeGrid *grid)
{
  IDE_ENTRY;
  for (guint i = 0; i < r_items->len; i++)
    {
      RestoreItem *item = &g_array_index (r_items, RestoreItem, i);
      IdeGridColumn *column;
      IdeFrame *stack;

      /* Ignore pages that couldn't be restored. */
      if (item->restored_page == NULL)
        continue;

      /* This relies on the fact that the items are sorted. */
      column = ide_grid_get_nth_column (grid, item->column);
      stack = _ide_grid_get_nth_stack_for_column (grid, column, item->row);

      gtk_container_add (GTK_CONTAINER (stack),
                         GTK_WIDGET (item->restored_page));
    }
  IDE_EXIT;
}

typedef struct
{
  IdeTask     *task;
  RestoreItem *item;
} RestorePage;

static void
on_session_addin_page_restored_cb (GObject      *object,
                                   GAsyncResult *result,
                                   gpointer      user_data)
{
  IdeSessionAddin *addin = (IdeSessionAddin *)object;
  RestorePage *r_page = user_data;
  g_autoptr(IdeTask) task = r_page->task;
  g_autoptr(GError) error = NULL;
  RestoreItem *item = r_page->item;
  Restore *r;

  IDE_ENTRY;

  g_assert (IDE_IS_SESSION_ADDIN (addin));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  r = ide_task_get_task_data (task);

  g_assert (r != NULL);
  g_assert (r->addins != NULL);
  g_assert (r->active > 0);
  g_assert (r->state != NULL);

  if (!(item->restored_page = ide_session_addin_restore_page_finish (addin, result, &error)))
    g_warning ("Couldn't restore page with addin %s: %s", G_OBJECT_TYPE_NAME (addin), error->message);

  r->active--;

  if (r->active == 0)
    {
      restore_pages_to_grid (r->pages, r->grid);

      ide_task_return_boolean (task, TRUE);
    }

  IDE_EXIT;
}

static IdeSessionAddin *
get_addin_for_name (GPtrArray  *addins,
                    const char *addin_name)
{
  GType addin_type = g_type_from_name (addin_name);
  for (guint i = 0; i < addins->len; i++)
    {
      if (G_OBJECT_TYPE (addins->pdata[i]) == addin_type)
        return addins->pdata[i];
    }
  return NULL;
}

static void
load_restore_items (Restore *r,
                    GArray  *items)
{
  GVariantIter iter;
  RestoreItem item;
  GVariant *page_state = NULL;

  g_assert (r != NULL);
  g_assert (r->state != NULL);
  g_assert (r->addins != NULL);
  g_assert (items != NULL);

  g_variant_iter_init (&iter, r->state);
  while ((page_state = g_variant_iter_next_value (&iter)))
    {
      const char *addin_name = NULL;

      g_variant_lookup (page_state, "column", "u", &item.column);
      g_variant_lookup (page_state, "row", "u", &item.row);
      g_variant_lookup (page_state, "depth", "u", &item.depth);
      g_variant_lookup (page_state, "addin_name", "&s", &addin_name);
      g_variant_lookup (page_state, "addin_page_state", "v", &item.state);

      item.addin = get_addin_for_name (r->addins, addin_name);
      g_array_append_val (items, item);

      g_variant_unref (page_state);
    }
}

static GVariant *
migrate_pre_api_rework (GVariant *pages_variant)
{
  GVariantIter iter;
  const char *uri = NULL;
  int column, row, depth;
  /* Freed in the loop. */
  GVariant *search_variant;

  GVariantDict version_wrapper_dict;
  GVariantBuilder addins_states;

  g_variant_dict_init (&version_wrapper_dict, NULL);
  /* Migrate old format to first version of the new format. */
  g_variant_dict_insert (&version_wrapper_dict, "version", "u", (guint32) 1);

  g_variant_builder_init (&addins_states, G_VARIANT_TYPE ("aa{sv}"));

  g_debug ("Handling migration of the project's session.gvariant, from prior to the Session API rework…");

  g_variant_iter_init (&iter, pages_variant);
  while (g_variant_iter_next (&iter, "(&siiiv)", &uri, &column, &row, &depth, &search_variant))
    {
      GVariantDict addin_state;
      GVariantDict editor_session_state;

      g_variant_dict_init (&addin_state, NULL);
      g_variant_dict_insert (&addin_state, "column", "u", (guint32) column);
      g_variant_dict_insert (&addin_state, "row", "u", (guint32) row);
      g_variant_dict_insert (&addin_state, "depth", "u", (guint32) depth);
      g_variant_dict_insert (&addin_state, "addin_name", "s", "GbpEditorSessionAddin");

      /* Since we need to migrate the data for the new API, let's also migrate to a dictionary
       * instead of a tuple, for greater flexibility and extensibility in the future.
       */
      g_variant_dict_init (&editor_session_state, NULL);
      g_variant_dict_insert (&editor_session_state, "uri", "s", uri);
      /* Unbox the search_variant since we don't want to bother with multiple levels of variants,
       * just have an a{sv}
       */
      g_variant_dict_insert_value (&editor_session_state, "search", g_variant_get_variant (search_variant));
      g_variant_dict_insert (&addin_state, "addin_page_state", "v", g_variant_dict_end (&editor_session_state));
      g_variant_builder_add_value (&addins_states, g_variant_dict_end (&addin_state));

      g_variant_unref (search_variant);
    }

  g_variant_dict_insert_value (&version_wrapper_dict, "data", g_variant_builder_end (&addins_states));

  g_debug ("Successfully migrated old session.gvariant to new format.");

  return g_variant_take_ref (g_variant_dict_end (&version_wrapper_dict));
}

static GVariant *
load_state_with_migrations (GBytes *bytes)
{
  g_autoptr(GVariant) variant = NULL;
  /* This is the value of the "data" key in the final @variant. */
  g_autoptr(GVariant) migrated_state = NULL;
  GVariantDict state;
  gboolean fully_migrated = FALSE;
  GVariant *old_api_state = NULL;

  g_assert (bytes != NULL);

  variant = g_variant_take_ref (g_variant_new_from_bytes (G_VARIANT_TYPE_VARDICT, bytes, FALSE));

  if (!variant)
    {
      g_warning ("Couldn't load the array of pages' states from session.gvariant!");
      return NULL;
    }

  g_variant_dict_init (&state, variant);

  /* Handle migrations from prior to the Session API rework, where there was only GbpEditorSessionAddin that used it */
  old_api_state = g_variant_dict_lookup_value (&state, "GbpEditorSessionAddin", G_VARIANT_TYPE ("a(siiiv)"));
  if (old_api_state)
    migrated_state = migrate_pre_api_rework (old_api_state);
  else
    migrated_state = g_steal_pointer (&variant);

  while (!fully_migrated)
    {
      guint32 version;
      g_autoptr(GVariant) versioned_data = NULL;

      if (!g_variant_lookup (migrated_state, "version", "u", &version))
        {
          g_warning ("session.gvariant isn't using the old format but doesn't have a version field, so cannot load it!");
          fully_migrated = TRUE;
          migrated_state = NULL;
          break;
        }

      if (!(versioned_data = g_variant_lookup_value (migrated_state, "data", G_VARIANT_TYPE ("aa{sv}"))))
        {
          g_warning ("session.gvariant had a version field but the actual versioned data wasn't found, so cannot load it!");
          fully_migrated = TRUE;
          migrated_state = NULL;
          break;
        }

      switch (version)
        {
          /* It's the current format so the rest of the code understands it natively. */
          case 1:
            migrated_state = g_steal_pointer (&migrated_state);
            fully_migrated = TRUE;
            break;

          default:
            g_warning ("Version %d of session.gvariant data is not known to Builder!", version);
            migrated_state = NULL;
            fully_migrated = TRUE;
        }
    }

  if (migrated_state)
    /* The current format (version 1) is an `aa{sv}` (array of dictionaries) with the dict's keys being:
     * guint32 column, row, depth;
     * char *addin_name;
     * GVariant *addin_page_state;
     */
    return g_variant_lookup_value (migrated_state, "data", NULL);
  else
    return NULL;
}

static void
on_session_cache_loaded_cb (GObject      *object,
                            GAsyncResult *result,
                            gpointer      user_data)
{
  GFile *file = (GFile *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  g_autoptr(GBytes) bytes = NULL;
  GArray *items = NULL;
  GCancellable *cancellable;
  Restore *r;

  IDE_ENTRY;

  g_assert (G_IS_FILE (file));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  r = ide_task_get_task_data (task);
  cancellable = ide_task_get_cancellable (task);

  g_assert (r != NULL);
  g_assert (r->addins != NULL);
  g_assert (r->addins->len > 0);
  g_assert (r->state == NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  if (!(bytes = g_file_load_bytes_finish (file, result, NULL, &error)))
    {
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
        ide_task_return_boolean (task, TRUE);
      else
        ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  if (g_bytes_get_size (bytes) == 0)
    {
      ide_task_return_boolean (task, TRUE);
      IDE_EXIT;
    }

  r->state = load_state_with_migrations (bytes);

  if (r->state == NULL)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_INVALID_DATA,
                                 "Failed to decode session state");
      IDE_EXIT;
    }

  items = g_array_new (FALSE, FALSE, sizeof (RestoreItem));
  g_array_set_clear_func (items, (GDestroyNotify)restore_item_clear);
  load_restore_items (r, items);
  r->pages = items;
  r->active = items->len;
  g_array_sort (items, compare_restore_items);

  for (guint i = 0; i < items->len; i++)
    {
      RestoreItem *item = &g_array_index (items, RestoreItem, i);
      RestorePage *r_page = g_slice_new0 (RestorePage);
      r_page->task = g_object_ref (task);
      r_page->item = item;

      ide_session_addin_restore_page_async (item->addin,
                                            item->state,
                                            cancellable,
                                            on_session_addin_page_restored_cb,
                                            r_page);
    }

  if (r->active == 0)
    {
      ide_task_return_boolean (task, TRUE);
      IDE_EXIT;
    }

  IDE_EXIT;
}

/**
 * ide_session_restore_async:
 * @self: an #IdeSession
 * @grid: an #IdeGrid
 * @cancellable: (nullable): a #GCancellable or %NULL
 * @callback: the callback to execute upon completion
 * @user_data: user data for callback
 *
 * This function will asynchronously restore the state of the project to
 * the point it was last saved (typically upon shutdown). This includes
 * open documents and editor splits to the degree possible. Adding support
 * for a new page type requires implementing an #IdeSessionAddin.
 *
 * Since: 41
 */
void
ide_session_restore_async (IdeSession          *self,
                           IdeGrid             *grid,
                           GCancellable        *cancellable,
                           GAsyncReadyCallback  callback,
                           gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GFile) file = NULL;
  g_autoptr(GSettings) settings = NULL;
  IdeContext *context;
  Restore *r;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_SESSION (self));
  g_return_if_fail (IDE_IS_GRID (grid));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_session_restore_async);

  r = g_slice_new0 (Restore);
  r->addins = self->addins;
  r->grid = grid;
  ide_task_set_task_data (task, r, restore_free);

  settings = g_settings_new ("org.gnome.builder");
  if (!g_settings_get_boolean (settings, "restore-previous-files"))
    {
      ide_task_return_boolean (task, TRUE);
      return;
    }

  context = ide_object_get_context (IDE_OBJECT (self));
  file = ide_context_cache_file (context, "session.gvariant", NULL);

  g_file_load_bytes_async (file,
                           cancellable,
                           on_session_cache_loaded_cb,
                           g_steal_pointer (&task));

  IDE_EXIT;
}

gboolean
ide_session_restore_finish (IdeSession    *self,
                            GAsyncResult  *result,
                            GError       **error)
{
  gboolean ret;
  Restore *r;
  GListModel *list;
  AutosaveGrid *autosave_grid;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_SESSION (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  r = ide_task_get_task_data (IDE_TASK (result));
  g_assert (r != NULL);
  list = G_LIST_MODEL (r->grid);

  autosave_grid = g_slice_new0 (AutosaveGrid);
  autosave_grid->grid = r->grid;
  autosave_grid->session = self;
  autosave_grid->session_autosave_source = 0;

  watch_pages_session_autosave (autosave_grid,
                                0, g_list_model_get_n_items (list));
  g_signal_connect_data (list,
                         "items-changed",
                         G_CALLBACK (on_grid_items_changed_cb),
                         autosave_grid,
                         autosave_grid_free,
                         0);

  ret = ide_task_propagate_boolean (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

static void
on_state_saved_to_cache_file_cb (GObject      *object,
                                 GAsyncResult *result,
                                 gpointer      user_data)
{
  GFile *file = (GFile *)object;
  g_autoptr(GError) error = NULL;
  g_autoptr(IdeTask) task = user_data;

  IDE_ENTRY;

  g_assert (G_IS_FILE (file));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!g_file_replace_contents_finish (file, result, NULL, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

static void
get_page_position (IdePage *page,
                   guint   *out_column,
                   guint   *out_row,
                   guint   *out_depth)
{
  GtkWidget *frame_pages_stack;
  GtkWidget *frame;
  GtkWidget *grid_column;
  GtkWidget *grid;

  g_assert (IDE_IS_PAGE (page));
  g_assert (out_column != NULL);
  g_assert (out_row != NULL);
  g_assert (out_depth != NULL);

  frame_pages_stack = gtk_widget_get_ancestor (GTK_WIDGET (page), GTK_TYPE_STACK);
  frame = gtk_widget_get_ancestor (GTK_WIDGET (frame_pages_stack), IDE_TYPE_FRAME);
  grid_column = gtk_widget_get_ancestor (GTK_WIDGET (frame), IDE_TYPE_GRID_COLUMN);
  grid = gtk_widget_get_ancestor (GTK_WIDGET (grid_column), IDE_TYPE_GRID);

  /* When this page is the currently visible one for this frame, we want to keep it on top when
   * restoring so that there's no need to switch back to the pages we were working on. We need to
   * do this because the stack's "position" child property only refers to the order in which the
   * pages were initially opened, not the most-recently-used order.
   */
  if (ide_frame_get_visible_child (IDE_FRAME (frame)) == page)
    {
      *out_depth = g_list_model_get_n_items (G_LIST_MODEL (frame));
    }
  else
    {
      gtk_container_child_get (GTK_CONTAINER (frame_pages_stack), GTK_WIDGET (page),
                               "position", out_depth,
                               NULL);
      *out_depth = MAX (*out_depth, 0);
    }

  gtk_container_child_get (GTK_CONTAINER (grid_column), GTK_WIDGET (frame),
                           "index", out_row,
                           NULL);
  *out_row = MAX (*out_row, 0);

  gtk_container_child_get (GTK_CONTAINER (grid), GTK_WIDGET (grid_column),
                           "index", out_column,
                           NULL);
  *out_column = MAX (*out_column, 0);
}

typedef struct {
  IdeTask *task;
  IdePage *page;
} SavePage;

static void
save_state_to_disk (IdeSession      *self,
                    IdeTask         *task,
                    GVariantBuilder *pages_state)
{
  g_autoptr(GVariant) state = NULL;
  g_autoptr(GBytes) bytes = NULL;
  g_autoptr(GFile) file = NULL;
  GCancellable *cancellable;
  IdeContext *context;
  GVariantDict final_dict;

  IDE_ENTRY;

  g_assert (IDE_IS_SESSION (self));
  g_assert (IDE_IS_TASK (task));
  g_assert (pages_state != NULL);

  cancellable = ide_task_get_cancellable (task);

  g_variant_dict_init (&final_dict, NULL);
  g_variant_dict_insert (&final_dict, "version", "u", (guint32) 1);
  g_variant_dict_insert_value (&final_dict, "data", g_variant_builder_end (pages_state));

  state = g_variant_ref_sink (g_variant_dict_end (&final_dict));
  bytes = g_variant_get_data_as_bytes (state);

#ifdef IDE_ENABLE_TRACE
  {
    g_autofree char *str = g_variant_print (state, TRUE);
    IDE_TRACE_MSG ("Saving session state to %s", str);
  }
#endif

  context = ide_object_get_context (IDE_OBJECT (self));
  file = ide_context_cache_file (context, "session.gvariant", NULL);

  if (!ide_task_return_error_if_cancelled (task))
    g_file_replace_contents_bytes_async (file,
                                         bytes,
                                         NULL,
                                         FALSE,
                                         G_FILE_CREATE_NONE,
                                         cancellable,
                                         on_state_saved_to_cache_file_cb,
                                         g_object_ref (task));

  IDE_EXIT;
}

static void
on_session_addin_page_saved_cb (GObject      *object,
                                GAsyncResult *result,
                                gpointer      user_data)
{
  IdeSessionAddin *addin = (IdeSessionAddin *)object;
  g_autoptr(GVariant) page_state = NULL;
  SavePage *save_page = user_data;
  g_autoptr(IdeTask) task = save_page->task;
  IdePage *page = save_page->page;
  g_autoptr(GError) error = NULL;
  IdeSession *self;
  Save *s;

  IDE_ENTRY;

  g_assert (IDE_IS_SESSION_ADDIN (addin));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  self = ide_task_get_source_object (task);
  s = ide_task_get_task_data (task);

  g_assert (IDE_IS_SESSION (self));
  g_assert (s != NULL);
  g_assert (s->active > 0);

  page_state = ide_session_addin_save_page_finish (addin, result, &error);

  if (error != NULL)
    g_warning ("Could not save page with addin %s: %s", G_OBJECT_TYPE_NAME (addin), error->message);

  if (page_state != NULL)
    {
      guint frame_column, frame_row, frame_depth;
      GVariantDict state_dict;

      g_assert (!g_variant_is_floating (page_state));

      get_page_position (page, &frame_column, &frame_row, &frame_depth);

      g_variant_dict_init (&state_dict, NULL);
      g_variant_dict_insert (&state_dict, "column", "u", frame_column);
      g_variant_dict_insert (&state_dict, "row", "u", frame_row);
      g_variant_dict_insert (&state_dict, "depth", "u", frame_depth);
      g_variant_dict_insert (&state_dict, "addin_name", "s", G_OBJECT_TYPE_NAME (addin));
      g_variant_dict_insert (&state_dict, "addin_page_state", "v", page_state);

      g_variant_builder_add_value (&s->pages_state, g_variant_dict_end (&state_dict));
    }

  g_slice_free (SavePage, save_page);

  s->active--;

  if (s->active == 0)
    save_state_to_disk (self, task, &s->pages_state);

  IDE_EXIT;
}

static void
foreach_page_in_grid_save_cb (GtkWidget *widget,
                              gpointer   user_data)
{
  IdePage *page = IDE_PAGE (widget);
  IdeTask *task = user_data;
  IdeSessionAddin *addin;
  SavePage *save_page = NULL;
  Save *s;

  g_assert (IDE_IS_PAGE (page));
  g_assert (IDE_IS_TASK (task));

  s = ide_task_get_task_data (task);

  g_assert (s != NULL);
  g_assert (s->addins != NULL);

  if (!(addin = find_suitable_addin_for_page (page, s->addins)))
    {
      /* It's not a saveable page. */
      s->active--;
      return;
    }

  save_page = g_slice_new0 (SavePage);
  save_page->task = g_object_ref (task);
  save_page->page = page;

  ide_session_addin_save_page_async (addin,
                                     page,
                                     ide_task_get_cancellable (task),
                                     on_session_addin_page_saved_cb,
                                     g_steal_pointer (&save_page));
}

/**
 * ide_session_save_async:
 * @self: an #IdeSession
 * @grid: an #IdeGrid
 * @cancellable: (nullable): a #GCancellable, or %NULL
 * @callback: a callback to execute upon completion
 * @user_data: user data for @callback
 *
 * This function will save the position and content of the pages in the @grid,
 * which can then be restored with ide_session_restore_async(), asking the
 * content of the pages to the appropriate #IdeSessionAddin.
 *
 * Since: 41
 */
void
ide_session_save_async (IdeSession          *self,
                        IdeGrid             *grid,
                        GCancellable        *cancellable,
                        GAsyncReadyCallback  callback,
                        gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  Save *s;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_SESSION (self));
  g_return_if_fail (IDE_IS_GRID (grid));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_session_save_async);

  s = g_slice_new0 (Save);
  s->addins = self->addins;
  s->grid = grid;
  s->active = ide_grid_count_pages (s->grid);

  g_variant_builder_init (&s->pages_state, G_VARIANT_TYPE ("aa{sv}"));
  ide_task_set_task_data (task, s, save_free);

  ide_grid_foreach_page (s->grid,
                         foreach_page_in_grid_save_cb,
                         task);

  g_assert (s != NULL);

  /* Save the empty pages state there too because it wouldn't have
   * been done in foreach_page_in_grid_save_cb() since there's no
   * pages to save.
   */
  if (s->active == 0)
    save_state_to_disk (self, task, &s->pages_state);

  IDE_EXIT;
}

gboolean
ide_session_save_finish (IdeSession    *self,
                         GAsyncResult  *result,
                         GError       **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_SESSION (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  ret = ide_task_propagate_boolean (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

IdeSession *
ide_session_new (void)
{
  return g_object_new (IDE_TYPE_SESSION, NULL);
}
