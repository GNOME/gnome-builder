/* ide-gtk.c
 *
 * Copyright 2015-2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-gtk"

#include "config.h"

#include <libide-threading.h>

#include "ide-animation.h"
#include "ide-gtk.h"

void
ide_gtk_window_present (GtkWindow *window)
{
  /* TODO: We need the last event time to do this properly. Until then,
   * we'll just fake some timing info to workaround wayland issues.
   */
  gtk_window_present_with_time (window, g_get_monotonic_time () / 1000L);
}

static void
ide_gtk_show_uri_on_window_cb (GObject      *object,
                               GAsyncResult *result,
                               gpointer      user_data)
{
  IdeSubprocess *subprocess = (IdeSubprocess *)object;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_SUBPROCESS (subprocess));
  g_assert (G_IS_ASYNC_RESULT (result));

  if (!ide_subprocess_wait_finish (subprocess, result, &error))
    g_warning ("Subprocess failed: %s", error->message);
}

gboolean
ide_gtk_show_uri_on_window (GtkWindow    *window,
                            const gchar  *uri,
                            gint64        timestamp,
                            GError      **error)
{
  g_return_val_if_fail (!window || GTK_IS_WINDOW (window), FALSE);
  g_return_val_if_fail (uri != NULL, FALSE);

  if (ide_is_flatpak ())
    {
      g_autoptr(IdeSubprocessLauncher) launcher = NULL;
      g_autoptr(IdeSubprocess) subprocess = NULL;

      /* We can't currently trust gtk_show_uri_on_window() because it tries
       * to open our HTML page with Builder inside our current flatpak
       * environment! We need to ensure this is fixed upstream, but it's
       * currently unclear how to do so since we register handles for html.
       */

      launcher = ide_subprocess_launcher_new (0);
      ide_subprocess_launcher_set_run_on_host (launcher, TRUE);
      ide_subprocess_launcher_set_clear_env (launcher, FALSE);
      ide_subprocess_launcher_push_argv (launcher, "xdg-open");
      ide_subprocess_launcher_push_argv (launcher, uri);

      if (!(subprocess = ide_subprocess_launcher_spawn (launcher, NULL, error)))
        return FALSE;

      ide_subprocess_wait_async (subprocess,
                                 NULL,
                                 ide_gtk_show_uri_on_window_cb,
                                 NULL);
    }
  else
    {
      /* XXX: Workaround for wayland timestamp issue */
      gtk_show_uri (window, uri, timestamp / 1000L);
    }

  return TRUE;
}

static gboolean
ide_gtk_progress_bar_tick_cb (gpointer data)
{
  GtkProgressBar *progress = data;

  g_assert (GTK_IS_PROGRESS_BAR (progress));

  gtk_progress_bar_pulse (progress);
  gtk_widget_queue_draw (GTK_WIDGET (progress));

  return G_SOURCE_CONTINUE;
}

void
ide_gtk_progress_bar_stop_pulsing (GtkProgressBar *progress)
{
  guint tick_id;

  g_return_if_fail (GTK_IS_PROGRESS_BAR (progress));

  tick_id = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (progress), "PULSE_ID"));

  if (tick_id != 0)
    {
      g_source_remove (tick_id);
      g_object_set_data (G_OBJECT (progress), "PULSE_ID", NULL);
    }

  gtk_progress_bar_set_fraction (progress, 0.0);
}

void
ide_gtk_progress_bar_start_pulsing (GtkProgressBar *progress)
{
  guint tick_id;

  g_return_if_fail (GTK_IS_PROGRESS_BAR (progress));

  if (g_object_get_data (G_OBJECT (progress), "PULSE_ID"))
    return;

  gtk_progress_bar_set_fraction (progress, 0.0);
  gtk_progress_bar_set_pulse_step (progress, .5);

  /* We want lower than the frame rate, because that is all that is needed */
  tick_id = g_timeout_add_full (G_PRIORITY_LOW,
                                500,
                                ide_gtk_progress_bar_tick_cb,
                                g_object_ref (progress),
                                g_object_unref);
  g_object_set_data (G_OBJECT (progress), "PULSE_ID", GUINT_TO_POINTER (tick_id));
  ide_gtk_progress_bar_tick_cb (progress);
}

static void
show_callback (gpointer data)
{
  g_object_set_data (data, "IDE_FADE_ANIMATION", NULL);
  g_object_unref (data);
}

static void
hide_callback (gpointer data)
{
  GtkWidget *widget = data;

  g_object_set_data (data, "IDE_FADE_ANIMATION", NULL);
  gtk_widget_hide (widget);
  gtk_widget_set_opacity (widget, 1.0);
  g_object_unref (widget);
}

void
ide_gtk_widget_show_with_fade (GtkWidget *widget)
{
  GdkFrameClock *frame_clock;
  IdeAnimation *anim;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (!gtk_widget_get_visible (widget))
    {
      anim = g_object_get_data (G_OBJECT (widget), "IDE_FADE_ANIMATION");
      if (anim != NULL)
        ide_animation_stop (anim);

      frame_clock = gtk_widget_get_frame_clock (widget);
      gtk_widget_set_opacity (widget, 0.0);
      gtk_widget_show (widget);
      anim = ide_object_animate_full (widget,
                                      IDE_ANIMATION_LINEAR,
                                      500,
                                      frame_clock,
                                      show_callback,
                                      g_object_ref (widget),
                                      "opacity", 1.0,
                                      NULL);
      g_object_set_data_full (G_OBJECT (widget),
                              "IDE_FADE_ANIMATION",
                              g_object_ref (anim),
                              g_object_unref);
    }
}

void
ide_gtk_widget_hide_with_fade (GtkWidget *widget)
{
  GdkFrameClock *frame_clock;
  IdeAnimation *anim;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (gtk_widget_get_visible (widget))
    {
      anim = g_object_get_data (G_OBJECT (widget), "IDE_FADE_ANIMATION");
      if (anim != NULL)
        ide_animation_stop (anim);

      frame_clock = gtk_widget_get_frame_clock (widget);
      anim = ide_object_animate_full (widget,
                                      IDE_ANIMATION_LINEAR,
                                      1000,
                                      frame_clock,
                                      hide_callback,
                                      g_object_ref (widget),
                                      "opacity", 0.0,
                                      NULL);
      g_object_set_data_full (G_OBJECT (widget),
                              "IDE_FADE_ANIMATION",
                              g_object_ref (anim),
                              g_object_unref);
    }
}

static gboolean
list_store_iter_middle (GtkListStore      *store,
                        const GtkTreeIter *begin,
                        const GtkTreeIter *end,
                        GtkTreeIter       *middle)
{
  g_assert (store != NULL);
  g_assert (begin != NULL);
  g_assert (end != NULL);
  g_assert (middle != NULL);
  g_assert (middle->stamp == begin->stamp);
  g_assert (middle->stamp == end->stamp);

  /*
   * middle MUST ALREADY BE VALID as it saves us some copying
   * as well as just makes things easier when binary searching.
   */

  middle->user_data = g_sequence_range_get_midpoint (begin->user_data, end->user_data);

  if (g_sequence_iter_is_end (middle->user_data))
    {
      middle->stamp = 0;
      return FALSE;
    }

  return TRUE;
}

static inline gboolean
list_store_iter_equal (const GtkTreeIter *a,
                       const GtkTreeIter *b)
{
  return a->user_data == b->user_data;
}

/**
 * ide_gtk_list_store_insert_sorted: (skip)
 * @store: A #GtkListStore
 * @iter: (out): A location for a #GtkTextIter
 * @key: A key to compare to when binary searching
 * @compare_column: the column containing the data to compare
 * @compare_func: (scope call) (closure compare_data): A callback to compare
 * @compare_data: data for @compare_func
 *
 * This function will binary search the contents of @store looking for the
 * location to insert a new row.
 *
 * @compare_column must be the index of a column that is a %G_TYPE_POINTER,
 * %G_TYPE_BOXED or %G_TYPE_OBJECT based column.
 *
 * @compare_func will be called with @key as the first parameter and the
 * value from the #GtkListStore row as the second parameter. The third and
 * final parameter is @compare_data.
 */
void
ide_gtk_list_store_insert_sorted (GtkListStore     *store,
                                  GtkTreeIter      *iter,
                                  gconstpointer     key,
                                  guint             compare_column,
                                  GCompareDataFunc  compare_func,
                                  gpointer          compare_data)
{
  GValue value = G_VALUE_INIT;
  gpointer (*get_func) (const GValue *) = NULL;
  GtkTreeModel *model = (GtkTreeModel *)store;
  GtkTreeIter begin;
  GtkTreeIter end;
  GtkTreeIter middle;
  guint n_children;
  gint cmpval = 0;
  GType type;

  g_return_if_fail (GTK_IS_LIST_STORE (store));
  g_return_if_fail (GTK_IS_LIST_STORE (model));
  g_return_if_fail (iter != NULL);
  g_return_if_fail (compare_column < gtk_tree_model_get_n_columns (GTK_TREE_MODEL (store)));
  g_return_if_fail (compare_func != NULL);

  type = gtk_tree_model_get_column_type (GTK_TREE_MODEL (store), compare_column);

  if (g_type_is_a (type, G_TYPE_POINTER))
    get_func = g_value_get_pointer;
  else if (g_type_is_a (type, G_TYPE_BOXED))
    get_func = g_value_get_boxed;
  else if (g_type_is_a (type, G_TYPE_OBJECT))
    get_func = g_value_get_object;
  else
    {
      g_warning ("%s() only supports pointer, boxed, or object columns",
                 G_STRFUNC);
      gtk_list_store_append (store, iter);
      return;
    }

  /* Try to get the first iter instead of calling n_children to
   * avoid walking the GSequence all the way to the right. If this
   * matches, we know there are some children.
   */
  if (!gtk_tree_model_get_iter_first (model, &begin))
    {
      gtk_list_store_append (store, iter);
      return;
    }

  n_children = gtk_tree_model_iter_n_children (model, NULL);
  if (!gtk_tree_model_iter_nth_child (model, &end, NULL, n_children - 1))
    g_assert_not_reached ();

  middle = begin;

  while (list_store_iter_middle (store, &begin, &end, &middle))
    {
      gtk_tree_model_get_value (model, &middle, compare_column, &value);
      cmpval = compare_func (key, get_func (&value), compare_data);
      g_value_unset (&value);

      if (cmpval == 0 || list_store_iter_equal (&begin, &end))
        break;

      if (cmpval < 0)
        {
          end = middle;

          if (!list_store_iter_equal (&begin, &end) &&
              !gtk_tree_model_iter_previous (model, &end))
            break;
        }
      else if (cmpval > 0)
        {
          begin = middle;

          if (!list_store_iter_equal (&begin, &end) &&
              !gtk_tree_model_iter_next (model, &begin))
            break;
        }
      else
        g_assert_not_reached ();
    }

  if (cmpval < 0)
    gtk_list_store_insert_before (store, iter, &middle);
  else
    gtk_list_store_insert_after (store, iter, &middle);
}

void
ide_gtk_widget_destroyed (GtkWidget  *widget,
                          GtkWidget **location)
{
  if (location != NULL)
    *location = NULL;
}
