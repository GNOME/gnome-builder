/* ide-search-popover.c
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

#define G_LOG_DOMAIN "ide-search-popover"

#include "config.h"

#include "ide-search-popover-private.h"
#include "ide-search-resources.h"
#include "ide-gui-global.h"

#define LONG_SEARCH_DELAY_MSEC  100
#define SHORT_SEARCH_DELAY_MSEC 50

struct _IdeSearchPopover
{
  GtkPopover          parent_instance;

  GCancellable       *cancellable;
  IdeSearchEngine    *search_engine;

  GtkSearchEntry     *entry;
  GtkSingleSelection *selection;
  GtkListView        *list_view;
  GtkWidget          *left;
  GtkWidget          *right;
  GtkRevealer        *preview_revealer;
  GtkWidget          *center;

  guint               queued_search;

  guint               activate_after_search : 1;
};

enum {
  PROP_0,
  PROP_SEARCH_ENGINE,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (IdeSearchPopover, ide_search_popover, GTK_TYPE_POPOVER)

static GParamSpec *properties [N_PROPS];

static void
ide_search_popover_cancel (IdeSearchPopover *self)
{
  g_assert (IDE_IS_SEARCH_POPOVER (self));

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);
  self->cancellable = g_cancellable_new ();
}

static void
ide_search_popover_activate (IdeSearchPopover *self,
                             IdeSearchResult  *result)
{
  IdeWorkspace *workspace;
  GtkWidget *last_focus = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_SEARCH_POPOVER (self));
  g_assert (IDE_IS_SEARCH_RESULT (result));

  /* We want GtkWindow:focus-widget because we don't care that the
   * popover has a GtkText focused for text entry. We want what was
   * last focused in the parent workspace window.
   */
  if ((workspace = ide_widget_get_workspace (GTK_WIDGET (self))))
    last_focus = gtk_window_get_focus (GTK_WINDOW (workspace));

  gtk_popover_popdown (GTK_POPOVER (self));

  ide_search_result_activate (result, last_focus);

  IDE_EXIT;
}

static void
ide_search_popover_hide_action (GtkWidget  *widget,
                                const char *action_name,
                                GVariant   *param)
{
  IdeWorkspace *workspace;
  IdePage *page;

  g_assert (IDE_IS_SEARCH_POPOVER (widget));

  workspace = ide_widget_get_workspace (widget);
  page = ide_workspace_get_most_recent_page (workspace);

  gtk_popover_popdown (GTK_POPOVER (widget));

  if (page != NULL)
    gtk_widget_grab_focus (GTK_WIDGET (page));
}

static void
ide_search_popover_set_search_engine (IdeSearchPopover *self,
                                      IdeSearchEngine  *search_engine)
{
  g_assert (IDE_IS_SEARCH_POPOVER (self));
  g_assert (IDE_IS_SEARCH_ENGINE (search_engine));

  g_set_object (&self->search_engine, search_engine);
}

static void
ide_search_popover_after_search (IdeSearchPopover *self)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_SEARCH_POPOVER (self));

  if (self->activate_after_search)
    {
      IdeSearchResult *selected = gtk_single_selection_get_selected_item (self->selection);

      g_assert (!selected || IDE_IS_SEARCH_RESULT (selected));

      self->activate_after_search = FALSE;

      if (selected != NULL)
        ide_search_popover_activate (self, selected);
    }
}

static void
ide_search_popover_search_cb (GObject      *object,
                              GAsyncResult *result,
                              gpointer      user_data)
{
  IdeSearchEngine *search_engine = (IdeSearchEngine *)object;
  g_autoptr(IdeSearchPopover) self = user_data;
  g_autoptr(IdeSearchResults) results = NULL;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_SEARCH_ENGINE (search_engine));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_SEARCH_POPOVER (self));

  results = ide_search_engine_search_finish (search_engine, result, &error);

  if (error != NULL)
    g_debug ("Search failed: %s", error->message);

  gtk_single_selection_set_model (self->selection, G_LIST_MODEL (results));

  ide_search_popover_after_search (self);

  IDE_EXIT;
}

static gboolean
ide_search_popover_search_source_func (gpointer data)
{
  IdeSearchPopover *self = data;
  GListModel *model;
  const char *query;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_SEARCH_POPOVER (self));

  self->queued_search = 0;

  if (self->search_engine == NULL)
    IDE_GOTO (failure);

  ide_search_popover_cancel (self);

  query = gtk_editable_get_text (GTK_EDITABLE (self->entry));

  if (ide_str_empty0 (query))
    IDE_GOTO (failure);

  /* Fast path to just filter our previous result set */
  if ((model = gtk_single_selection_get_model (self->selection)) &&
      IDE_IS_SEARCH_RESULTS (model) &&
      ide_search_results_refilter (IDE_SEARCH_RESULTS (model), query))
    {
      ide_search_popover_after_search (self);
      IDE_RETURN (G_SOURCE_REMOVE);
    }

  ide_search_engine_search_async (self->search_engine,
                                  query,
                                  0,
                                  self->cancellable,
                                  ide_search_popover_search_cb,
                                  g_object_ref (self));

  IDE_RETURN (G_SOURCE_REMOVE);

failure:
  self->activate_after_search = FALSE;
  gtk_single_selection_set_model (self->selection, NULL);

  IDE_RETURN (G_SOURCE_REMOVE);
}

static void
ide_search_popover_queue_search (IdeSearchPopover *self)
{
  const char *text;
  guint delay;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_SEARCH_POPOVER (self));

  g_clear_handle_id (&self->queued_search, g_source_remove);

  text = gtk_editable_get_text (GTK_EDITABLE (self->entry));

  if (self->activate_after_search)
    delay = 0;
  else if (strlen (text) < 3)
    delay = LONG_SEARCH_DELAY_MSEC;
  else
    delay = SHORT_SEARCH_DELAY_MSEC;

  self->queued_search = g_timeout_add (delay,
                                       ide_search_popover_search_source_func,
                                       self);
}

static void
ide_search_popover_search_changed_cb (IdeSearchPopover *self,
                                      GtkEditable      *editable)
{
  IDE_ENTRY;

  g_assert (IDE_IS_SEARCH_POPOVER (self));
  g_assert (GTK_IS_EDITABLE (editable));

  ide_search_popover_queue_search (self);

  IDE_EXIT;
}

static void
ide_search_popover_activate_cb (IdeSearchPopover *self,
                                guint             position,
                                GtkListView      *list_view)
{
  g_autoptr(IdeSearchResult) result = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_SEARCH_POPOVER (self));
  g_assert (GTK_IS_LIST_VIEW (list_view));

  result = g_list_model_get_item (G_LIST_MODEL (self->selection), position);

  g_assert (result != NULL);
  g_assert (IDE_IS_SEARCH_RESULT (result));

  ide_search_popover_activate (self, result);

  IDE_EXIT;
}

static void
ide_search_popover_entry_activate_cb (IdeSearchPopover *self,
                                      GtkEditable      *editable)
{
  g_assert (IDE_IS_SEARCH_POPOVER (self));
  g_assert (GTK_IS_EDITABLE (editable));

  /* Delay activation until the result comes back. Just send off
   * another search out of simplifity here. When it comes back we
   * activate. That way we always get the same result no matter if
   * a search was in-progress while activate happened.
   */
  self->activate_after_search = TRUE;

  ide_search_popover_queue_search (self);
}

static void
ide_search_popover_move_action (GtkWidget  *widget,
                                const char *action_name,
                                GVariant   *param)
{
  IdeSearchPopover *self = (IdeSearchPopover *)widget;
  guint selected;
  int dir;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_SEARCH_POPOVER (self));
  g_assert (g_variant_is_of_type (param, G_VARIANT_TYPE_INT32));

  selected = gtk_single_selection_get_selected (self->selection);
  dir = g_variant_get_int32 (param);

  if (dir < 0)
    {
      if (selected < ABS (dir))
        selected = 0;
      else
        selected -= ABS (dir);
    }
  else
    {
      selected += dir;
    }

  if (selected < g_list_model_get_n_items (G_LIST_MODEL (self->selection)))
    gtk_single_selection_set_selected (self->selection, selected);

  IDE_EXIT;
}

static void
ide_search_popover_next_match_cb (IdeSearchPopover *self,
                                  GtkSearchEntry   *entry)
{
  guint selected;

  g_assert (IDE_IS_SEARCH_POPOVER (self));
  g_assert (GTK_IS_SEARCH_ENTRY (entry));

  selected = gtk_single_selection_get_selected (self->selection);

  if (selected + 1 < g_list_model_get_n_items (G_LIST_MODEL (self->selection)))
    gtk_single_selection_set_selected (self->selection, selected + 1);
}

static void
ide_search_popover_previous_match_cb (IdeSearchPopover *self,
                                      GtkSearchEntry   *entry)
{
  guint selected;

  g_assert (IDE_IS_SEARCH_POPOVER (self));
  g_assert (GTK_IS_SEARCH_ENTRY (entry));

  selected = gtk_single_selection_get_selected (self->selection);

  if (selected > 0)
    gtk_single_selection_set_selected (self->selection, selected - 1);
}

static void
ide_search_popover_show (GtkWidget *widget)
{
  IdeSearchPopover *self = (IdeSearchPopover *)widget;

  g_assert (IDE_IS_SEARCH_POPOVER (self));

  GTK_WIDGET_CLASS (ide_search_popover_parent_class)->show (widget);

  gtk_widget_grab_focus (GTK_WIDGET (self->entry));
}

static gboolean
ide_search_popover_grab_focus (GtkWidget *widget)
{
  IdeSearchPopover *self = (IdeSearchPopover *)widget;

  g_assert (IDE_IS_SEARCH_POPOVER (self));

  return gtk_widget_grab_focus (GTK_WIDGET (self->entry));
}

static void
ide_search_popover_dispose (GObject *object)
{
  IdeSearchPopover *self = (IdeSearchPopover *)object;

  g_clear_handle_id (&self->queued_search, g_source_remove);

  g_clear_object (&self->cancellable);
  g_clear_object (&self->search_engine);

  G_OBJECT_CLASS (ide_search_popover_parent_class)->dispose (object);
}

static void
ide_search_popover_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  IdeSearchPopover *self = IDE_SEARCH_POPOVER (object);

  switch (prop_id)
    {
    case PROP_SEARCH_ENGINE:
      g_value_set_object (value, self->search_engine);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_search_popover_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  IdeSearchPopover *self = IDE_SEARCH_POPOVER (object);

  switch (prop_id)
    {
    case PROP_SEARCH_ENGINE:
      ide_search_popover_set_search_engine (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_search_popover_class_init (IdeSearchPopoverClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = ide_search_popover_dispose;
  object_class->get_property = ide_search_popover_get_property;
  object_class->set_property = ide_search_popover_set_property;

  widget_class->grab_focus = ide_search_popover_grab_focus;
  widget_class->show = ide_search_popover_show;

  properties [PROP_SEARCH_ENGINE] =
    g_param_spec_object ("search-engine",
                         "Search Engine",
                         "The search engine for the popover",
                         IDE_TYPE_SEARCH_ENGINE,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  g_resources_register (ide_search_get_resource ());

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libide-gui/ui/ide-search-popover.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeSearchPopover, center);
  gtk_widget_class_bind_template_child (widget_class, IdeSearchPopover, entry);
  gtk_widget_class_bind_template_child (widget_class, IdeSearchPopover, left);
  gtk_widget_class_bind_template_child (widget_class, IdeSearchPopover, list_view);
  gtk_widget_class_bind_template_child (widget_class, IdeSearchPopover, preview_revealer);
  gtk_widget_class_bind_template_child (widget_class, IdeSearchPopover, right);
  gtk_widget_class_bind_template_child (widget_class, IdeSearchPopover, selection);
  gtk_widget_class_bind_template_callback (widget_class, ide_search_popover_activate_cb);
  gtk_widget_class_bind_template_callback (widget_class, ide_search_popover_entry_activate_cb);
  gtk_widget_class_bind_template_callback (widget_class, ide_search_popover_search_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, ide_search_popover_next_match_cb);
  gtk_widget_class_bind_template_callback (widget_class, ide_search_popover_previous_match_cb);

  gtk_widget_class_install_action (widget_class, "search.hide", NULL, ide_search_popover_hide_action);
  gtk_widget_class_install_action (widget_class, "search.move", "i", ide_search_popover_move_action);
}

static void
ide_search_popover_init (IdeSearchPopover *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_popover_set_offset (GTK_POPOVER (self), 180, 0);
}

GtkWidget *
ide_search_popover_new (IdeSearchEngine *search_engine)
{
  g_return_val_if_fail (IDE_IS_SEARCH_ENGINE (search_engine), NULL);

  return g_object_new (IDE_TYPE_SEARCH_POPOVER,
                       "search-engine", search_engine,
                       NULL);
}

void
ide_search_popover_present (IdeSearchPopover *self,
                            int               parent_width,
                            int               parent_height)
{
  GtkRequisition left, right;

  g_return_if_fail (IDE_IS_SEARCH_POPOVER (self));

  gtk_widget_get_preferred_size (GTK_WIDGET (self->left), &left, NULL);
  gtk_widget_get_preferred_size (GTK_WIDGET (self->preview_revealer), &right, NULL);

  gtk_popover_set_offset (GTK_POPOVER (self), -(left.width/2) + (right.width/2), 0);
  gtk_popover_set_pointing_to (GTK_POPOVER (self), &(GdkRectangle) { parent_width/2, 100, 1, 1 });
  gtk_popover_present (GTK_POPOVER (self));
}
