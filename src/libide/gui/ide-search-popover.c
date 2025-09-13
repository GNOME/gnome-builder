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

#include <glib/gi18n.h>

#include "ide-search-popover-private.h"
#include "ide-search-resources.h"
#include "ide-gui-global.h"

#define LONG_SEARCH_DELAY_MSEC  100
#define SHORT_SEARCH_DELAY_MSEC 50

struct _IdeSearchPopover
{
  AdwDialog           parent_instance;

  GCancellable       *cancellable;
  IdeSearchEngine    *search_engine;
  GSettings          *settings;

  AdwSpinner         *spinner;
  GtkText            *text;
  GtkListView        *list_view;
  AdwBin             *preview_bin;
  GtkSingleSelection *selection;
  GtkStack           *stack;

  IdeSearchCategory   last_category;

  guint               queued_search;

  guint               sequence;

  guint               activate_after_search : 1;
  guint               disposed : 1;
  guint               has_preview : 1;
  guint               show_preview : 1;
};

enum {
  PROP_0,
  PROP_SEARCH_ENGINE,
  PROP_SHOW_PREVIEW,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (IdeSearchPopover, ide_search_popover, ADW_TYPE_DIALOG)

static GParamSpec *properties [N_PROPS];

typedef struct
{
  IdeSearchPopover *self;
  guint sequence;
} SearchState;

static void
search_state_free (SearchState *state)
{
  g_clear_object (&state->self);
  g_free (state);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (SearchState, search_state_free);

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

  /* To make this all more predictable, we use the most recent page. That
   * means that panels need to expose their actions more globally if they
   * want to be accessible from the action search provider.
   *
   * This is much more predictable than trying to apply from random widgets
   * which might have had focus before we displayed the popover.
   *
   * Additionally, it means that you need to make your page actions available
   * properly on the page, not just within a widget inside them.
   */

  if ((workspace = ide_widget_get_workspace (GTK_WIDGET (self))))
    last_focus = GTK_WIDGET (ide_workspace_get_most_recent_page (workspace));

  if (last_focus == NULL)
    last_focus = GTK_WIDGET (workspace);

  adw_dialog_close (ADW_DIALOG (self));

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

  adw_dialog_close (ADW_DIALOG (widget));

  if (page != NULL)
    gtk_widget_grab_focus (GTK_WIDGET (page));
}

static void
ide_search_popover_set_search_engine (IdeSearchPopover *self,
                                      IdeSearchEngine  *search_engine)
{
  g_assert (IDE_IS_SEARCH_POPOVER (self));
  g_assert (IDE_IS_SEARCH_ENGINE (search_engine));

  if (g_set_object (&self->search_engine, search_engine))
    {
      g_autoptr(GListModel) model = NULL;

      if (search_engine != NULL)
        model = ide_search_engine_list_providers (search_engine);
    }
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
  g_autoptr(SearchState) state = user_data;
  g_autoptr(IdeSearchResults) results = NULL;
  g_autoptr(GError) error = NULL;
  IdeSearchPopover *self;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_SEARCH_ENGINE (search_engine));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (state != NULL);
  g_assert (IDE_IS_SEARCH_POPOVER (state->self));

  self = state->self;

  results = ide_search_engine_search_finish (search_engine, result, &error);

  if (error != NULL)
    g_debug ("Search failed: %s", error->message);

  if (self->sequence != state->sequence)
    {
      g_debug ("Search (%u) no longer valid (current %u), ignoring",
               state->sequence, self->sequence);
      return;
    }

  gtk_widget_set_visible (GTK_WIDGET (self->spinner), FALSE);

  gtk_single_selection_set_model (self->selection, G_LIST_MODEL (results));

  gtk_stack_set_visible_child_name (self->stack, "results");

  ide_search_popover_after_search (self);

  IDE_EXIT;
}

static gboolean
ide_search_popover_search_source_func (gpointer data)
{
  g_autofree char *query_stripped = NULL;
  IdeSearchCategory category;
  IdeSearchPopover *self = data;
  SearchState *state;
  const char *query;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_SEARCH_POPOVER (self));

  self->queued_search = 0;

  if (self->search_engine == NULL)
    IDE_GOTO (failure);

  ide_search_popover_cancel (self);

  query = gtk_editable_get_text (GTK_EDITABLE (self->text));

  if (ide_str_empty0 (query))
    IDE_GOTO (failure);

  switch (*query)
    {
    case '?':
      category = IDE_SEARCH_CATEGORY_DOCUMENTATION;
      query++;
      break;

    case '@':
      category = IDE_SEARCH_CATEGORY_SYMBOLS;
      query++;
      break;

    case '>':
      /* TODO: Maybe commands here too? */
      category = IDE_SEARCH_CATEGORY_ACTIONS;
      query++;
      break;

    case '~':
      category = IDE_SEARCH_CATEGORY_FILES;
      query++;
      break;

    default:
      category = IDE_SEARCH_CATEGORY_EVERYTHING;
      break;
    }

  query = query_stripped = g_strstrip (g_strdup (query));

  if (ide_str_empty0 (query))
    IDE_GOTO (failure);

#if 0
  /* Drop refiltering because the performance is extremely bad for
   * lazy loaded search results. Instead, it would be better to
   * allow for the provider to refilter their listmodel like I
   * did in GtkSourceCompletion.
   */

  GListModel *model;
  if (category == self->last_category &&
      !ide_str_empty0 (query) &&
      (model = gtk_single_selection_get_model (self->selection)) &&
      IDE_IS_SEARCH_RESULTS (model) &&
      ide_search_results_refilter (IDE_SEARCH_RESULTS (model), query))
    {
      ide_search_popover_after_search (self);
      IDE_RETURN (G_SOURCE_REMOVE);
    }
#endif

  self->last_category = category;

  state = g_new0 (SearchState, 1);
  state->self = g_object_ref (self);
  state->sequence = ++self->sequence;

  gtk_widget_set_visible (GTK_WIDGET (self->spinner), TRUE);

  ide_search_engine_search_async (self->search_engine,
                                  category,
                                  query,
                                  0,
                                  self->cancellable,
                                  ide_search_popover_search_cb,
                                  state);

  IDE_RETURN (G_SOURCE_REMOVE);

failure:
  self->activate_after_search = FALSE;
  gtk_single_selection_set_model (self->selection, NULL);
  gtk_stack_set_visible_child_name (self->stack, "empty");

  IDE_RETURN (G_SOURCE_REMOVE);
}

static void
ide_search_popover_queue_search (IdeSearchPopover *self)
{
  const char *text;
  guint delay;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_SEARCH_POPOVER (self));

  if (self->disposed)
    return;

  g_clear_handle_id (&self->queued_search, g_source_remove);

  text = gtk_editable_get_text (GTK_EDITABLE (self->text));

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
ide_search_popover_search_focus (GtkWidget  *widget,
                                 const char *action_name,
                                 GVariant   *param)
{
  IdeSearchPopover *self = IDE_SEARCH_POPOVER (widget);

  gtk_widget_grab_focus (GTK_WIDGET (self->text));
  gtk_editable_select_region (GTK_EDITABLE (self->text), 0, -1);
}

static void
ide_search_popover_move_action (GtkWidget  *widget,
                                const char *action_name,
                                GVariant   *param)
{
  IdeSearchPopover *self = (IdeSearchPopover *)widget;
  guint selected;
  int dir = 1;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_SEARCH_POPOVER (self));
  g_assert (!param || g_variant_is_of_type (param, G_VARIANT_TYPE_INT32));

  if (param != NULL)
    dir = g_variant_get_int32 (param);
  else if (ide_str_equal0 (action_name, "search.move-up"))
    dir = -1;

  selected = gtk_single_selection_get_selected (self->selection);

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
    {
      GtkScrollInfo *scroll_info;

      gtk_single_selection_set_selected (self->selection, selected);

      scroll_info = gtk_scroll_info_new ();
      gtk_scroll_info_set_enable_vertical (scroll_info, TRUE);

      gtk_list_view_scroll_to (self->list_view, selected, 0, scroll_info);
    }

  IDE_EXIT;
}

static void
ide_search_popover_set_preview (IdeSearchPopover *self,
                                IdeSearchPreview *preview)
{
  gboolean can_show_preview;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_SEARCH_POPOVER (self));
  g_assert (!preview || IDE_IS_SEARCH_PREVIEW (preview));

  self->has_preview = preview != NULL;
  adw_bin_set_child (self->preview_bin, GTK_WIDGET (preview));
  can_show_preview = self->has_preview && self->show_preview;

  gtk_widget_set_visible (GTK_WIDGET (self->preview_bin), can_show_preview);

  IDE_EXIT;
}

static void
ide_search_popover_selection_changed_cb (IdeSearchPopover   *self,
                                         GParamSpec         *pspec,
                                         GtkSingleSelection *selection)
{
  IdeSearchPreview *preview = NULL;
  IdeSearchResult *result;
  IdeContext *context;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_SEARCH_POPOVER (self));
  g_assert (GTK_IS_SINGLE_SELECTION (selection));

  context = ide_widget_get_context (GTK_WIDGET (self));

  if ((result = gtk_single_selection_get_selected_item (selection)))
    preview = ide_search_result_load_preview (result, context);

  ide_search_popover_set_preview (self, preview);

  IDE_EXIT;
}

static void
ide_search_popover_category_changed_cb (IdeSearchPopover *self)
{
  IDE_ENTRY;
  ide_search_popover_queue_search (self);
  IDE_EXIT;
}

static void
ide_search_popover_map (GtkWidget *widget)
{
  IdeSearchPopover *self = (IdeSearchPopover *)widget;

  g_assert (IDE_IS_SEARCH_POPOVER (self));

  GTK_WIDGET_CLASS (ide_search_popover_parent_class)->map (widget);

  gtk_widget_grab_focus (GTK_WIDGET (self->text));
  gtk_editable_select_region (GTK_EDITABLE (self->text), 0, -1);
}

static gboolean
ide_search_popover_grab_focus (GtkWidget *widget)
{
  IdeSearchPopover *self = (IdeSearchPopover *)widget;

  g_assert (IDE_IS_SEARCH_POPOVER (self));

  return gtk_widget_grab_focus (GTK_WIDGET (self->text));
}

static char *
null_to_empty (gpointer    instance,
               const char *string)
{
  if (string != NULL)
    return g_strdup (string);

  return g_strdup ("");
}

static void
ide_search_popover_dispose (GObject *object)
{
  IdeSearchPopover *self = (IdeSearchPopover *)object;

  self->disposed = TRUE;

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

    case PROP_SHOW_PREVIEW:
      g_value_set_boolean (value, ide_search_popover_get_show_preview (self));
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

    case PROP_SHOW_PREVIEW:
      ide_search_popover_set_show_preview (self, g_value_get_boolean (value));
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
  widget_class->map = ide_search_popover_map;

  properties [PROP_SHOW_PREVIEW] =
    g_param_spec_boolean ("show-preview", NULL, NULL,
                          TRUE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_SEARCH_ENGINE] =
    g_param_spec_object ("search-engine",
                         "Search Engine",
                         "The search engine for the popover",
                         IDE_TYPE_SEARCH_ENGINE,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  g_resources_register (ide_search_get_resource ());

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libide-gui/ui/ide-search-popover.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeSearchPopover, list_view);
  gtk_widget_class_bind_template_child (widget_class, IdeSearchPopover, preview_bin);
  gtk_widget_class_bind_template_child (widget_class, IdeSearchPopover, selection);
  gtk_widget_class_bind_template_child (widget_class, IdeSearchPopover, spinner);
  gtk_widget_class_bind_template_child (widget_class, IdeSearchPopover, stack);
  gtk_widget_class_bind_template_child (widget_class, IdeSearchPopover, text);
  gtk_widget_class_bind_template_callback (widget_class, ide_search_popover_activate_cb);
  gtk_widget_class_bind_template_callback (widget_class, ide_search_popover_category_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, ide_search_popover_entry_activate_cb);
  gtk_widget_class_bind_template_callback (widget_class, ide_search_popover_search_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, ide_search_popover_selection_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, null_to_empty);

  gtk_widget_class_install_action (widget_class, "search.hide", NULL, ide_search_popover_hide_action);
  gtk_widget_class_install_action (widget_class, "search.move", "i", ide_search_popover_move_action);
  gtk_widget_class_install_action (widget_class, "search.move-up", NULL, ide_search_popover_move_action);
  gtk_widget_class_install_action (widget_class, "search.move-down", NULL, ide_search_popover_move_action);
  gtk_widget_class_install_action (widget_class, "search.focus", NULL, ide_search_popover_search_focus);

  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_k, GDK_CONTROL_MASK, "search.focus", NULL);
  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_l, GDK_CONTROL_MASK, "search.focus", NULL);
}

static void
ide_search_popover_init (IdeSearchPopover *self)
{
  self->show_preview = TRUE;
  self->settings = g_settings_new ("org.gnome.builder");

  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
ide_search_popover_new (IdeSearchEngine *search_engine)
{
  g_return_val_if_fail (IDE_IS_SEARCH_ENGINE (search_engine), NULL);

  return g_object_new (IDE_TYPE_SEARCH_POPOVER,
                       "search-engine", search_engine,
                       NULL);
}

gboolean
ide_search_popover_get_show_preview (IdeSearchPopover *self)
{
  g_return_val_if_fail (IDE_IS_SEARCH_POPOVER (self), FALSE);

  return self->show_preview;
}

void
ide_search_popover_set_show_preview (IdeSearchPopover *self,
                                     gboolean          show_preview)
{
  g_return_if_fail (IDE_IS_SEARCH_POPOVER (self));

  show_preview = !!show_preview;

  if (show_preview != self->show_preview)
    {
      self->show_preview = show_preview;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SHOW_PREVIEW]);

      ide_search_popover_selection_changed_cb (self, NULL, self->selection);
    }
}
