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
#include "ide-search-popover-group-private.h"
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

  GListStore         *groups;

  GtkWidget          *left;
  GtkWidget          *right;
  GtkWidget          *center;

  GtkSearchEntry     *entry;
  GtkListView        *list_view;
  AdwBin             *preview_bin;
  AdwWindowTitle     *preview_title;
  GtkToggleButton    *preview_toggle;
  GtkRevealer        *preview_revealer;
  GtkListBox         *providers_list_box;
  GtkSingleSelection *selection;

  IdeSearchCategory   last_category;

  guint               queued_search;

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

static void buildable_iface_init (GtkBuildableIface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (IdeSearchPopover, ide_search_popover, ADW_TYPE_DIALOG,
                               G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE, buildable_iface_init))

static GParamSpec *properties [N_PROPS];
static GtkBuildableIface *parent_buildable_iface;

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
group_header_func (GtkListBoxRow *row,
                   GtkListBoxRow *before,
                   gpointer       user_data)
{
  IdeSearchPopoverGroup *before_group;

  if (before == NULL)
    return;

  if ((before_group = g_object_get_data (G_OBJECT (before), "GROUP")) &&
      ide_str_equal (("_Everything"), ide_search_popover_group_get_title (before_group)))
    gtk_list_box_row_set_header (row,
                                 g_object_new (GTK_TYPE_SEPARATOR,
                                               "orientation", GTK_ORIENTATION_HORIZONTAL,
                                               NULL));
}

static GtkWidget *
create_group_row (gpointer item,
                  gpointer user_data)
{
  IdeSearchPopoverGroup *group = item;
  const char *title;
  const char *icon_name;
  GtkWidget *box;
  GtkWidget *image;
  GtkWidget *label;
  GtkWidget *row;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_SEARCH_POPOVER_GROUP (group));

  title = ide_search_popover_group_get_title (group);
  icon_name = ide_search_popover_group_get_icon_name (group);

  box = g_object_new (GTK_TYPE_BOX,
                      "orientation", GTK_ORIENTATION_HORIZONTAL,
                      "spacing", 9,
                      NULL);
  image = g_object_new (GTK_TYPE_IMAGE,
                        "icon-name", icon_name,
                        NULL);
  label = g_object_new (GTK_TYPE_LABEL,
                        "label", title,
                        "xalign", .0f,
                        "ellipsize", PANGO_ELLIPSIZE_END,
                        "use-underline", TRUE,
                        NULL);
  gtk_box_append (GTK_BOX (box), image);
  gtk_box_append (GTK_BOX (box), label);

  row = g_object_new (GTK_TYPE_LIST_BOX_ROW,
                      "css-classes", IDE_STRV_INIT ("sidebar-row"),
                      "child", box,
                      NULL);

  g_object_set_data_full (G_OBJECT (row),
                          "GROUP",
                          g_object_ref (group),
                          g_object_unref);

  return GTK_WIDGET (row);
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
  IdeSearchCategory category = IDE_SEARCH_CATEGORY_EVERYTHING;
  IdeSearchPopoverGroup *group;
  IdeSearchPopover *self = data;
  GtkListBoxRow *row;
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

  /* Get the category for the query */
  if ((row = gtk_list_box_get_selected_row (self->providers_list_box)) &&
      (group = g_object_get_data (G_OBJECT (row), "GROUP")) &&
      IDE_IS_SEARCH_POPOVER_GROUP (group))
    category = ide_search_popover_group_get_category (group);

  /* Fast path to just filter our previous result set */
  if (category == self->last_category &&
      (model = gtk_single_selection_get_model (self->selection)) &&
      IDE_IS_SEARCH_RESULTS (model) &&
      ide_search_results_refilter (IDE_SEARCH_RESULTS (model), query))
    {
      ide_search_popover_after_search (self);
      IDE_RETURN (G_SOURCE_REMOVE);
    }

  self->last_category = category;

  ide_search_engine_search_async (self->search_engine,
                                  category,
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

  if (self->disposed)
    return;

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
ide_search_popover_search_focus (GtkWidget  *widget,
                                 const char *action_name,
                                 GVariant   *param)
{
  IdeSearchPopover *self = IDE_SEARCH_POPOVER (widget);

  gtk_widget_grab_focus (GTK_WIDGET (self->entry));
  gtk_editable_select_region (GTK_EDITABLE (self->entry), 0, -1);
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
ide_search_popover_set_preview (IdeSearchPopover *self,
                                IdeSearchPreview *preview)
{
  const char *title = NULL;
  const char *subtitle = NULL;
  gboolean can_show_preview;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_SEARCH_POPOVER (self));
  g_assert (!preview || IDE_IS_SEARCH_PREVIEW (preview));

  self->has_preview = preview != NULL;
  adw_bin_set_child (self->preview_bin, GTK_WIDGET (preview));
  can_show_preview = self->has_preview && self->show_preview;

  gtk_revealer_set_reveal_child (self->preview_revealer, can_show_preview);

  if (preview != NULL)
    {
      title = ide_search_preview_get_title (preview);
      subtitle = ide_search_preview_get_subtitle (preview);
    }

  /* TODO: We might want to bind these properties */

  adw_window_title_set_title (self->preview_title, title);
  adw_window_title_set_subtitle (self->preview_title, subtitle);

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
ide_search_popover_show (GtkWidget *widget)
{
  IdeSearchPopover *self = (IdeSearchPopover *)widget;

  g_assert (IDE_IS_SEARCH_POPOVER (self));

  GTK_WIDGET_CLASS (ide_search_popover_parent_class)->show (widget);

  gtk_widget_grab_focus (GTK_WIDGET (self->entry));
  gtk_editable_select_region (GTK_EDITABLE (self->entry), 0, -1);
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

  self->disposed = TRUE;

  g_clear_handle_id (&self->queued_search, g_source_remove);

  g_clear_object (&self->cancellable);
  g_clear_object (&self->search_engine);
  g_clear_object (&self->groups);

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
  widget_class->show = ide_search_popover_show;

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
  gtk_widget_class_bind_template_child (widget_class, IdeSearchPopover, center);
  gtk_widget_class_bind_template_child (widget_class, IdeSearchPopover, entry);
  gtk_widget_class_bind_template_child (widget_class, IdeSearchPopover, left);
  gtk_widget_class_bind_template_child (widget_class, IdeSearchPopover, list_view);
  gtk_widget_class_bind_template_child (widget_class, IdeSearchPopover, preview_bin);
  gtk_widget_class_bind_template_child (widget_class, IdeSearchPopover, preview_revealer);
  gtk_widget_class_bind_template_child (widget_class, IdeSearchPopover, preview_toggle);
  gtk_widget_class_bind_template_child (widget_class, IdeSearchPopover, preview_title);
  gtk_widget_class_bind_template_child (widget_class, IdeSearchPopover, providers_list_box);
  gtk_widget_class_bind_template_child (widget_class, IdeSearchPopover, right);
  gtk_widget_class_bind_template_child (widget_class, IdeSearchPopover, selection);
  gtk_widget_class_bind_template_callback (widget_class, ide_search_popover_activate_cb);
  gtk_widget_class_bind_template_callback (widget_class, ide_search_popover_category_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, ide_search_popover_entry_activate_cb);
  gtk_widget_class_bind_template_callback (widget_class, ide_search_popover_search_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, ide_search_popover_next_match_cb);
  gtk_widget_class_bind_template_callback (widget_class, ide_search_popover_previous_match_cb);
  gtk_widget_class_bind_template_callback (widget_class, ide_search_popover_selection_changed_cb);

  gtk_widget_class_install_action (widget_class, "search.hide", NULL, ide_search_popover_hide_action);
  gtk_widget_class_install_action (widget_class, "search.move", "i", ide_search_popover_move_action);
  gtk_widget_class_install_action (widget_class, "search.focus", NULL, ide_search_popover_search_focus);

  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_k, GDK_CONTROL_MASK, "search.focus", NULL);
  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_l, GDK_CONTROL_MASK, "search.focus", NULL);

  g_type_ensure (IDE_TYPE_SEARCH_POPOVER_GROUP);
}

static void
ide_search_popover_init (IdeSearchPopover *self)
{
  self->show_preview = TRUE;
  self->groups = g_list_store_new (IDE_TYPE_SEARCH_POPOVER_GROUP);
  self->settings = g_settings_new ("org.gnome.builder");

  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_list_box_set_header_func (self->providers_list_box,
                                group_header_func, self, NULL);
  gtk_list_box_bind_model (self->providers_list_box,
                           G_LIST_MODEL (self->groups),
                           create_group_row, self, NULL);

  g_settings_bind (self->settings, "preview-search-results",
                   self->preview_toggle, "active",
                   G_SETTINGS_BIND_DEFAULT);
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

static void
ide_search_popover_add_child (GtkBuildable *buildable,
                              GtkBuilder   *builder,
                              GObject      *object,
                              const char   *type)
{
  IdeSearchPopover *self = IDE_SEARCH_POPOVER (buildable);

  if (IDE_IS_SEARCH_POPOVER_GROUP (object))
    g_list_store_append (self->groups, object);
  else
    parent_buildable_iface->add_child (buildable, builder, object, type);
}

static void
buildable_iface_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);
  iface->add_child = ide_search_popover_add_child;
}
