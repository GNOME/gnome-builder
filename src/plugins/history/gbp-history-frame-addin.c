/* gbp-history-frame-addin.c
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

#define G_LOG_DOMAIN "gbp-history-frame-addin"

#include "gbp-history-frame-addin.h"

#define MAX_HISTORY_ITEMS   20
#define NEARBY_LINES_THRESH 10

struct _GbpHistoryFrameAddin
{
  GObject         parent_instance;

  GListStore     *back_store;
  GListStore     *forward_store;

  GtkBox         *controls;
  GtkButton      *previous_button;
  GtkButton      *next_button;

  IdeFrame *stack;

  guint           navigating;
};

static void
gbp_history_frame_addin_update (GbpHistoryFrameAddin *self)
{
  gboolean has_items;

  g_assert (GBP_IS_HISTORY_FRAME_ADDIN (self));

  has_items = g_list_model_get_n_items (G_LIST_MODEL (self->back_store)) > 0;
  dzl_gtk_widget_action_set (GTK_WIDGET (self->controls),
                             "history", "move-previous-edit",
                             "enabled", has_items,
                             NULL);

  has_items = g_list_model_get_n_items (G_LIST_MODEL (self->forward_store)) > 0;
  dzl_gtk_widget_action_set (GTK_WIDGET (self->controls),
                             "history", "move-next-edit",
                             "enabled", has_items,
                             NULL);

#if 0
  g_print ("Backward\n");

  for (guint i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (self->back_store)); i++)
    {
      g_autoptr(GbpHistoryItem) item = g_list_model_get_item (G_LIST_MODEL (self->back_store), i);

      g_print ("%s\n", gbp_history_item_get_label (item));
    }

  g_print ("Forward\n");

  for (guint i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (self->forward_store)); i++)
    {
      g_autoptr(GbpHistoryItem) item = g_list_model_get_item (G_LIST_MODEL (self->forward_store), i);

      g_print ("%s\n", gbp_history_item_get_label (item));
    }
#endif
}

static void
gbp_history_frame_addin_navigate (GbpHistoryFrameAddin *self,
                                         GbpHistoryItem             *item)
{
  g_autoptr(IdeLocation) location = NULL;
  GtkWidget *editor;

  g_assert (GBP_IS_HISTORY_FRAME_ADDIN (self));
  g_assert (GBP_IS_HISTORY_ITEM (item));

  location = gbp_history_item_get_location (item);
  editor = gtk_widget_get_ancestor (GTK_WIDGET (self->controls), IDE_TYPE_EDITOR_SURFACE);
  ide_editor_surface_focus_location (IDE_EDITOR_SURFACE (editor), location);

  gbp_history_frame_addin_update (self);
}

static gboolean
item_is_nearby (IdeEditorPage  *editor,
                GbpHistoryItem *item)
{
  GtkTextIter insert;
  IdeBuffer *buffer;
  GFile *buffer_file;
  GFile *item_file;
  gint buffer_line;
  gint item_line;

  g_assert (IDE_IS_EDITOR_PAGE (editor));
  g_assert (GBP_IS_HISTORY_ITEM (item));

  buffer = ide_editor_page_get_buffer (editor);

  /* Make sure this is the same file */
  buffer_file = ide_buffer_get_file (buffer);
  item_file = gbp_history_item_get_file (item);
  if (!g_file_equal (buffer_file, item_file))
    return FALSE;

  /* Check if the lines are nearby */
  ide_buffer_get_selection_bounds (buffer, &insert, NULL);
  buffer_line = gtk_text_iter_get_line (&insert);
  item_line = gbp_history_item_get_line (item);

  return ABS (buffer_line - item_line) < NEARBY_LINES_THRESH;
}

static void
move_previous_edit_action (GSimpleAction *action,
                           GVariant      *param,
                           gpointer       user_data)
{
  GbpHistoryFrameAddin *self = user_data;
  IdePage *current;
  GListModel *model;
  guint n_items;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (GBP_IS_HISTORY_FRAME_ADDIN (self));
  g_assert (self->stack != NULL);

  model = G_LIST_MODEL (self->back_store);
  n_items = g_list_model_get_n_items (model);
  current = ide_frame_get_visible_child (self->stack);

  /*
   * The tip of the backward jumplist could be very close to
   * where we are now. So keep skipping backwards until the
   * item isn't near our current position.
   */

  self->navigating++;

  for (guint i = n_items; i > 0; i--)
    {
      g_autoptr(GbpHistoryItem) item = g_list_model_get_item (model, i - 1);

      g_list_store_remove (self->back_store, i - 1);
      g_list_store_insert (self->forward_store, 0, item);

      if (!IDE_IS_EDITOR_PAGE (current) ||
          !item_is_nearby (IDE_EDITOR_PAGE (current), item))
        {
          gbp_history_frame_addin_navigate (self, item);
          break;
        }
    }

  self->navigating--;
}

static void
move_next_edit_action (GSimpleAction *action,
                       GVariant      *param,
                       gpointer       user_data)
{
  GbpHistoryFrameAddin *self = user_data;
  IdePage *current;
  GListModel *model;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (GBP_IS_HISTORY_FRAME_ADDIN (self));

  model = G_LIST_MODEL (self->forward_store);
  current = ide_frame_get_visible_child (self->stack);

  self->navigating++;

  while (g_list_model_get_n_items (model) > 0)
    {
      g_autoptr(GbpHistoryItem) item = g_list_model_get_item (model, 0);

      g_list_store_remove (self->forward_store, 0);
      g_list_store_append (self->back_store, item);

      if (!IDE_IS_EDITOR_PAGE (current) ||
          !item_is_nearby (IDE_EDITOR_PAGE (current), item))
        {
          gbp_history_frame_addin_navigate (self, item);
          break;
        }
    }

  self->navigating--;
}

static const GActionEntry entries[] = {
  { "move-previous-edit", move_previous_edit_action },
  { "move-next-edit", move_next_edit_action },
};

static void
gbp_history_frame_addin_load (IdeFrameAddin *addin,
                                     IdeFrame      *stack)
{
  GbpHistoryFrameAddin *self = (GbpHistoryFrameAddin *)addin;
  g_autoptr(GSimpleActionGroup) actions = NULL;
  GtkWidget *header;

  g_assert (GBP_IS_HISTORY_FRAME_ADDIN (addin));
  g_assert (IDE_IS_FRAME (stack));

  self->stack = stack;

  actions = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (actions),
                                   entries,
                                   G_N_ELEMENTS (entries),
                                   self);
  gtk_widget_insert_action_group (GTK_WIDGET (stack),
                                  "history",
                                  G_ACTION_GROUP (actions));

  header = ide_frame_get_titlebar (stack);

  self->controls = g_object_new (GTK_TYPE_BOX,
                                 "orientation", GTK_ORIENTATION_HORIZONTAL,
                                 "sensitive", FALSE,
                                 "visible", TRUE,
                                 NULL);
  g_signal_connect (self->controls,
                    "destroy",
                    G_CALLBACK (gtk_widget_destroyed),
                    &self->controls);
  dzl_gtk_widget_add_style_class (GTK_WIDGET (self->controls), "linked");
  gtk_container_add_with_properties (GTK_CONTAINER (header), GTK_WIDGET (self->controls),
                                     "priority", -100,
                                     NULL);

  self->previous_button = g_object_new (GTK_TYPE_BUTTON,
                                        "action-name", "history.move-previous-edit",
                                        "child", g_object_new (GTK_TYPE_IMAGE,
                                                               "icon-name", "go-previous-symbolic",
                                                               "visible", TRUE,
                                                               NULL),
                                        "visible", TRUE,
                                        NULL);
  g_signal_connect (self->previous_button,
                    "destroy",
                    G_CALLBACK (gtk_widget_destroyed),
                    &self->previous_button);
  gtk_container_add (GTK_CONTAINER (self->controls), GTK_WIDGET (self->previous_button));

  self->next_button = g_object_new (GTK_TYPE_BUTTON,
                                    "action-name", "history.move-next-edit",
                                    "child", g_object_new (GTK_TYPE_IMAGE,
                                                           "icon-name", "go-next-symbolic",
                                                           "visible", TRUE,
                                                           NULL),
                                    "visible", TRUE,
                                    NULL);
  g_signal_connect (self->next_button,
                    "destroy",
                    G_CALLBACK (gtk_widget_destroyed),
                    &self->next_button);
  gtk_container_add (GTK_CONTAINER (self->controls), GTK_WIDGET (self->next_button));

  gbp_history_frame_addin_update (self);
}

static void
gbp_history_frame_addin_unload (IdeFrameAddin *addin,
                                       IdeFrame      *stack)
{
  GbpHistoryFrameAddin *self = (GbpHistoryFrameAddin *)addin;

  g_assert (GBP_IS_HISTORY_FRAME_ADDIN (addin));
  g_assert (IDE_IS_FRAME (stack));

  gtk_widget_insert_action_group (GTK_WIDGET (stack), "history", NULL);

  g_clear_object (&self->back_store);
  g_clear_object (&self->forward_store);

  if (self->controls != NULL)
    gtk_widget_destroy (GTK_WIDGET (self->controls));
  if (self->next_button != NULL)
    gtk_widget_destroy (GTK_WIDGET (self->next_button));
  if (self->previous_button != NULL)
    gtk_widget_destroy (GTK_WIDGET (self->previous_button));

  self->stack = NULL;
}

static void
gbp_history_frame_addin_set_view (IdeFrameAddin *addin,
                                         IdePage       *view)
{
  GbpHistoryFrameAddin *self = (GbpHistoryFrameAddin *)addin;

  g_assert (GBP_IS_HISTORY_FRAME_ADDIN (self));
  g_assert (!view || IDE_IS_PAGE (view));

  gtk_widget_set_sensitive (GTK_WIDGET (self->controls), IDE_IS_EDITOR_PAGE (view));
}

static void
frame_addin_iface_init (IdeFrameAddinInterface *iface)
{
  iface->load = gbp_history_frame_addin_load;
  iface->unload = gbp_history_frame_addin_unload;
  iface->set_page = gbp_history_frame_addin_set_view;
}

G_DEFINE_TYPE_WITH_CODE (GbpHistoryFrameAddin, gbp_history_frame_addin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_FRAME_ADDIN,
                                                frame_addin_iface_init))

static void
gbp_history_frame_addin_class_init (GbpHistoryFrameAddinClass *klass)
{
}

static void
gbp_history_frame_addin_init (GbpHistoryFrameAddin *self)
{
  self->back_store = g_list_store_new (GBP_TYPE_HISTORY_ITEM);
  self->forward_store = g_list_store_new (GBP_TYPE_HISTORY_ITEM);
}

static void
move_forward_to_back_store (GbpHistoryFrameAddin *self)
{
  IDE_ENTRY;

  g_assert (GBP_IS_HISTORY_FRAME_ADDIN (self));

  /* Be certain we're not disposed */
  if (self->forward_store == NULL || self->back_store == NULL)
    IDE_EXIT;

  while (g_list_model_get_n_items (G_LIST_MODEL (self->forward_store)))
    {
      g_autoptr(GbpHistoryItem) item = NULL;

      item = g_list_model_get_item (G_LIST_MODEL (self->forward_store), 0);
      g_list_store_remove (self->forward_store, 0);
      g_list_store_append (self->back_store, item);
    }

  IDE_EXIT;
}

static void
gbp_history_frame_addin_remove_dups (GbpHistoryFrameAddin *self)
{
  guint n_items;

  g_assert (GBP_IS_HISTORY_FRAME_ADDIN (self));
  g_assert (self->forward_store != NULL);
  g_assert (g_list_model_get_n_items (G_LIST_MODEL (self->forward_store)) == 0);

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->back_store));

  /* Start from the oldest history item and work our way to the most
   * recent item. Try to find any items later in the jump list which
   * we can coallesce with. If so, remove the entry, preferring the
   * more recent item.
   */

  for (guint i = 0; i < n_items; i++)
    {
      GbpHistoryItem *item;

    try_again:
      item = g_list_model_get_item (G_LIST_MODEL (self->back_store), i);

      for (guint j = n_items; (j - 1) > i; j--)
        {
          g_autoptr(GbpHistoryItem) recent = NULL;

          recent = g_list_model_get_item (G_LIST_MODEL (self->back_store), j - 1);

          g_assert (recent != item);

          if (gbp_history_item_chain (recent, item))
            {
              g_list_store_remove (self->back_store, i);
              g_object_unref (item);
              n_items--;
              goto try_again;
            }
        }

      g_object_unref (item);
    }
}

void
gbp_history_frame_addin_push (GbpHistoryFrameAddin *self,
                                     GbpHistoryItem             *item)
{
  guint n_items;

  IDE_ENTRY;

  g_return_if_fail (GBP_IS_HISTORY_FRAME_ADDIN (self));
  g_return_if_fail (GBP_IS_HISTORY_ITEM (item));
  g_return_if_fail (self->back_store != NULL);
  g_return_if_fail (self->forward_store != NULL);
  g_return_if_fail (self->stack != NULL);

  /* Ignore while we are navigating */
  if (self->navigating != 0)
    return;

  /* Move all of our forward marks to the backward list */
  move_forward_to_back_store (self);

  /* Now add our new item to the list */
  g_list_store_append (self->back_store, item);

  /* Now remove dups in the list */
  gbp_history_frame_addin_remove_dups (self);

  /* Truncate from head if necessary */
  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->back_store));
  if (n_items >= MAX_HISTORY_ITEMS)
    g_list_store_remove (self->back_store, 0);

  gbp_history_frame_addin_update (self);

  IDE_EXIT;
}
