/* ide-tweaks-panel-list.c
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

#define G_LOG_DOMAIN "ide-tweaks-panel-list"

#include "config.h"

#include <libide-search.h>

#include "ide-marshal.h"

#include "ide-tweaks-factory-private.h"
#include "ide-tweaks-model-private.h"
#include "ide-tweaks-page.h"
#include "ide-tweaks-panel-list-private.h"
#include "ide-tweaks-panel-list-row-private.h"
#include "ide-tweaks-section.h"

typedef struct
{
  IdePatternSpec *spec;
} SearchInfo;

struct _IdeTweaksPanelList
{
  AdwNavigationPage   parent_instance;

  IdeTweaksItem      *item;
  IdeTweaksItem      *selected;

  GtkListBox         *list_box;
  AdwBin             *search_bin;

  char               *last_search;
  GListModel         *model;
  GtkFilterListModel *filtered;

  SearchInfo         *info;
};

enum {
  PROP_0,
  PROP_ITEM,
  PROP_SEARCH_MODE,
  PROP_SELECTION_MODE,
  N_PROPS
};

enum {
  PAGE_ACTIVATED,
  N_SIGNALS
};

static void ide_tweaks_panel_list_reset_search (IdeTweaksPanelList *self);

G_DEFINE_FINAL_TYPE (IdeTweaksPanelList, ide_tweaks_panel_list, ADW_TYPE_NAVIGATION_PAGE)

static GParamSpec *properties [N_PROPS];
static guint signals [N_SIGNALS];

static void
search_info_finalize (gpointer data)
{
  SearchInfo *si = data;

  g_clear_pointer (&si->spec, ide_pattern_spec_unref);
}

static void
search_info_unref (SearchInfo *si)
{
  g_rc_box_release_full (si, search_info_finalize);
}

static gboolean
search_filter_func (gpointer itemptr,
                    gpointer user_data)
{
  IdeTweaksItem *item = itemptr;
  SearchInfo *si = user_data;

  return ide_tweaks_item_match (item, si->spec);
}

static IdeTweaksItemVisitResult
panel_list_visitor (IdeTweaksItem *item,
                    gpointer       user_data)
{
  static GType types[] = { G_TYPE_INVALID, G_TYPE_INVALID };

  if G_UNLIKELY (types[0] == G_TYPE_INVALID)
    {
      types[0] = IDE_TYPE_TWEAKS_PAGE;
      types[1] = IDE_TYPE_TWEAKS_SECTION;
    }

  if (item == user_data)
    return IDE_TWEAKS_ITEM_VISIT_RECURSE;

  if (IDE_IS_TWEAKS_SECTION (item))
    return IDE_TWEAKS_ITEM_VISIT_RECURSE;

  if (IDE_IS_TWEAKS_PAGE (item))
    return IDE_TWEAKS_ITEM_VISIT_ACCEPT_AND_CONTINUE;

  if (IDE_IS_TWEAKS_FACTORY (item) &&
      _ide_tweaks_factory_is_one_of (IDE_TWEAKS_FACTORY (item), types, G_N_ELEMENTS (types)))
    return IDE_TWEAKS_ITEM_VISIT_RECURSE;

  return IDE_TWEAKS_ITEM_VISIT_CONTINUE;
}

static GtkWidget *
ide_tweaks_panel_list_create_row_cb (gpointer item,
                                     gpointer user_data)
{
  return g_object_new (IDE_TYPE_TWEAKS_PANEL_LIST_ROW,
                       "item", item,
                       NULL);
}

static void
ide_tweaks_panel_list_set_item (IdeTweaksPanelList *self,
                                IdeTweaksItem      *item)
{
  g_assert (IDE_IS_TWEAKS_PANEL_LIST (self));
  g_assert (IDE_IS_TWEAKS_ITEM (item));

  if (g_set_object (&self->item, item))
    {
      ide_tweaks_panel_list_reset_search (self);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ITEM]);
    }
}

static gboolean
section_equal (IdeTweaksItem *section_a,
               IdeTweaksItem *section_b)
{
  const char *title_a;
  const char *title_b;

  if (section_a == section_b)
    return TRUE;

  if (section_a == NULL || section_b == NULL)
    return FALSE;

  title_a = ide_tweaks_section_get_title (IDE_TWEAKS_SECTION (section_a));
  title_b = ide_tweaks_section_get_title (IDE_TWEAKS_SECTION (section_b));

  if (title_a == NULL && title_b != NULL)
    return FALSE;

  if (title_a != NULL && title_b == NULL)
    return FALSE;

  return g_strcmp0 (title_a, title_b) == 0;
}

static void
ide_tweaks_panel_list_header_func (IdeTweaksPanelListRow *row,
                                   IdeTweaksPanelListRow *before,
                                   gpointer               user_data)
{
  IdeTweaksItem *before_item = NULL;
  IdeTweaksItem *row_item;
  IdeTweaksItem *before_section = NULL;
  IdeTweaksItem *row_section = NULL;
  GtkSeparator *separator = NULL;
  GtkLabel *label = NULL;
  GtkBox *box = NULL;

  g_assert (!before || IDE_IS_TWEAKS_PANEL_LIST_ROW (before));
  g_assert (IDE_IS_TWEAKS_PANEL_LIST_ROW (row));

  if (before != NULL)
    before_item = ide_tweaks_panel_list_row_get_item (before);
  row_item = ide_tweaks_panel_list_row_get_item (row);

  g_assert (IDE_IS_TWEAKS_PAGE (row_item));
  g_assert (!before_item || IDE_IS_TWEAKS_PAGE (before_item));

  if (before_item != NULL)
    before_section = ide_tweaks_page_get_section (IDE_TWEAKS_PAGE (before_item));
  row_section = ide_tweaks_page_get_section (IDE_TWEAKS_PAGE (row_item));

  if (before_section == NULL ||
      (row_section != NULL && !section_equal (before_section, row_section)))
    {
      if (row_section != NULL)
        {
          const char *title = ide_tweaks_section_get_title (IDE_TWEAKS_SECTION (row_section));
          gboolean show_header = ide_tweaks_section_get_show_header (IDE_TWEAKS_SECTION (row_section));

          if (show_header && title != NULL)
            label = g_object_new (GTK_TYPE_LABEL,
                                  "css-classes", IDE_STRV_INIT ("dim-label", "heading"),
                                  "label", title,
                                  "margin-bottom", 6,
                                  "margin-start", 18,
                                  "margin-top", 12,
                                  "xalign", .0f,
                                  NULL);
        }

      if (before_section != NULL)
        separator = g_object_new (GTK_TYPE_SEPARATOR,
                                  "orientation", GTK_ORIENTATION_HORIZONTAL,
                                  NULL);
    }

  if (label == NULL)
    {
      gtk_list_box_row_set_header (GTK_LIST_BOX_ROW (row), GTK_WIDGET (separator));
      return;
    }

  g_assert (label != NULL);

  box = g_object_new (GTK_TYPE_BOX,
                      "margin-top", before_section ? 4 : 0,
                      "orientation", GTK_ORIENTATION_VERTICAL,
                      NULL);
  if (separator != NULL)
    {
      /* We can't automatically style separators when they are in a box, so add margins manually */
      gtk_widget_set_margin_start (GTK_WIDGET (separator), 6);
      gtk_widget_set_margin_end (GTK_WIDGET (separator), 6);
      gtk_box_append (box, GTK_WIDGET (separator));
    }
  gtk_box_append (box, GTK_WIDGET (label));

  gtk_list_box_row_set_header (GTK_LIST_BOX_ROW (row), GTK_WIDGET (box));
}

static void
ide_tweaks_panel_list_row_activated_cb (IdeTweaksPanelList    *self,
                                        IdeTweaksPanelListRow *row,
                                        GtkListBox            *list_box)
{
  IdeTweaksItem *page;

  g_assert (IDE_IS_TWEAKS_PANEL_LIST (self));
  g_assert (IDE_IS_TWEAKS_PANEL_LIST_ROW (row));
  g_assert (GTK_IS_LIST_BOX (list_box));

  page = ide_tweaks_panel_list_row_get_item (row);

  g_assert (page != NULL);
  g_assert (IDE_IS_TWEAKS_PAGE (page));

  g_set_object (&self->selected, page);

  g_signal_emit (self, signals [PAGE_ACTIVATED], 0, page);
}

static void
ide_tweaks_panel_list_reset_search (IdeTweaksPanelList *self)
{
  g_autoptr(IdeTweaksModel) model = NULL;

  g_assert (IDE_IS_TWEAKS_PANEL_LIST (self));

  g_clear_object (&self->filtered);
  g_clear_pointer (&self->last_search, g_free);

  if (self->item != NULL)
    model = ide_tweaks_model_new (self->item, panel_list_visitor, self->item, NULL);

  g_set_object (&self->model, G_LIST_MODEL (model));

  gtk_list_box_bind_model (self->list_box,
                           G_LIST_MODEL (model),
                           ide_tweaks_panel_list_create_row_cb,
                           NULL, NULL);
}

static void
on_search_changed_cb (IdeTweaksPanelList *self,
                      GtkEditable        *entry)
{
  const char *text;

  g_assert (IDE_IS_TWEAKS_PANEL_LIST (self));
  g_assert (GTK_IS_EDITABLE (entry));

  text = gtk_editable_get_text (entry);

  if (ide_str_empty0 (text))
    {
      ide_tweaks_panel_list_reset_search (self);
      return;
    }

  g_clear_pointer (&self->info->spec, ide_pattern_spec_unref);
  self->info->spec = ide_pattern_spec_new (text);

  if (self->filtered)
    {
      GtkFilter *filter = gtk_filter_list_model_get_filter (GTK_FILTER_LIST_MODEL (self->filtered));

      if (g_str_has_prefix (text, self->last_search))
        gtk_filter_changed (filter, GTK_FILTER_CHANGE_MORE_STRICT);
      else if (g_str_has_prefix (self->last_search, text))
        gtk_filter_changed (filter, GTK_FILTER_CHANGE_LESS_STRICT);
      else
        gtk_filter_changed (filter, GTK_FILTER_CHANGE_DIFFERENT);
    }
  else
    {
      GtkCustomFilter *filter;

      filter = gtk_custom_filter_new (search_filter_func,
                                      g_rc_box_acquire (self->info),
                                      (GDestroyNotify)search_info_unref);
      self->filtered = gtk_filter_list_model_new (g_object_ref (self->model), GTK_FILTER (filter));
    }

  g_set_str (&self->last_search, text);

  gtk_list_box_bind_model (self->list_box,
                           G_LIST_MODEL (self->filtered),
                           ide_tweaks_panel_list_create_row_cb,
                           NULL, NULL);
}

static void
ide_tweaks_panel_list_dispose (GObject *object)
{
  IdeTweaksPanelList *self = (IdeTweaksPanelList *)object;

  g_clear_pointer (&self->info, search_info_unref);

  g_clear_object (&self->model);
  g_clear_object (&self->filtered);
  g_clear_pointer (&self->last_search, g_free);

  g_clear_object (&self->item);
  g_clear_object (&self->selected);

  G_OBJECT_CLASS (ide_tweaks_panel_list_parent_class)->dispose (object);
}

static void
ide_tweaks_panel_list_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  IdeTweaksPanelList *self = IDE_TWEAKS_PANEL_LIST (object);

  switch (prop_id)
    {
    case PROP_ITEM:
      g_value_set_object (value, ide_tweaks_panel_list_get_item (self));
      break;

    case PROP_SEARCH_MODE:
      g_value_set_boolean (value, ide_tweaks_panel_list_get_search_mode (self));
      break;

    case PROP_SELECTION_MODE:
      g_value_set_enum (value, ide_tweaks_panel_list_get_selection_mode (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_tweaks_panel_list_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  IdeTweaksPanelList *self = IDE_TWEAKS_PANEL_LIST (object);

  switch (prop_id)
    {
    case PROP_ITEM:
      ide_tweaks_panel_list_set_item (self, g_value_get_object (value));
      break;

    case PROP_SEARCH_MODE:
      ide_tweaks_panel_list_set_search_mode (self, g_value_get_boolean (value));
      break;

    case PROP_SELECTION_MODE:
      ide_tweaks_panel_list_set_selection_mode (self, g_value_get_enum (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_tweaks_panel_list_class_init (IdeTweaksPanelListClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = ide_tweaks_panel_list_dispose;
  object_class->get_property = ide_tweaks_panel_list_get_property;
  object_class->set_property = ide_tweaks_panel_list_set_property;

  properties [PROP_ITEM] =
    g_param_spec_object ("item", NULL, NULL,
                         IDE_TYPE_TWEAKS_ITEM,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_SEARCH_MODE] =
    g_param_spec_boolean ("search-mode", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties[PROP_SELECTION_MODE] =
    g_param_spec_enum ("selection-mode", NULL, NULL,
                       GTK_TYPE_SELECTION_MODE,
                       GTK_SELECTION_SINGLE,
                       (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libide-tweaks/ide-tweaks-panel-list.ui");
  gtk_widget_class_set_css_name (widget_class, "IdeTweaksPanelList");
  gtk_widget_class_bind_template_child (widget_class, IdeTweaksPanelList, list_box);
  gtk_widget_class_bind_template_child (widget_class, IdeTweaksPanelList, search_bin);
  gtk_widget_class_bind_template_callback (widget_class, ide_tweaks_panel_list_row_activated_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_search_changed_cb);

  signals [PAGE_ACTIVATED] =
    g_signal_new ("page-activated",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  ide_marshal_VOID__OBJECT,
                  G_TYPE_NONE,
                  1,
                  IDE_TYPE_TWEAKS_PAGE | G_SIGNAL_TYPE_STATIC_SCOPE);
  g_signal_set_va_marshaller (signals [PAGE_ACTIVATED],
                              G_TYPE_FROM_CLASS (klass),
                              ide_marshal_VOID__OBJECTv);
}

static void
ide_tweaks_panel_list_init (IdeTweaksPanelList *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->info = g_rc_box_new0 (SearchInfo);

  gtk_list_box_set_header_func (self->list_box,
                                (GtkListBoxUpdateHeaderFunc)ide_tweaks_panel_list_header_func,
                                NULL, NULL);
}

GtkWidget *
ide_tweaks_panel_list_new (IdeTweaksItem *item)
{
  g_return_val_if_fail (!item || IDE_IS_TWEAKS_ITEM (item), NULL);

  return g_object_new (IDE_TYPE_TWEAKS_PANEL_LIST,
                       "item", item,
                       NULL);
}

/**
 * ide_tweaks_panel_list_get_item:
 *
 * Gets the parent item of the panel list. Children of this
 * item are what are displayed in the panel list.
 *
 * Returns: (transfer none) (nullable): an #IdeTweaksItem or %NULL
 */
IdeTweaksItem *
ide_tweaks_panel_list_get_item (IdeTweaksPanelList *self)
{
  g_return_val_if_fail (IDE_IS_TWEAKS_PANEL_LIST (self), NULL);

  return self->item;
}

void
ide_tweaks_panel_list_select_first (IdeTweaksPanelList *self)
{
  g_return_if_fail (IDE_IS_TWEAKS_PANEL_LIST (self));

  for (GtkWidget *child = gtk_widget_get_first_child (GTK_WIDGET (self->list_box));
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      if (GTK_IS_LIST_BOX_ROW (child))
        {
          gtk_widget_activate (GTK_WIDGET (child));
          break;
        }
    }
}

void
ide_tweaks_panel_list_select_item (IdeTweaksPanelList *self,
                                   IdeTweaksItem      *item)
{
  g_return_if_fail (IDE_IS_TWEAKS_PANEL_LIST (self));
  g_return_if_fail (IDE_IS_TWEAKS_ITEM (item));

  for (GtkWidget *child = gtk_widget_get_first_child (GTK_WIDGET (self->list_box));
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      IdeTweaksPanelListRow *row;

      if (!IDE_IS_TWEAKS_PANEL_LIST_ROW (child))
        continue;

      row = IDE_TWEAKS_PANEL_LIST_ROW (child);

      if (item == ide_tweaks_panel_list_row_get_item (row))
        {
          gtk_widget_activate (child);
          break;
        }
    }
}

gboolean
ide_tweaks_panel_list_get_search_mode (IdeTweaksPanelList *self)
{
  g_return_val_if_fail (IDE_IS_TWEAKS_PANEL_LIST (self), FALSE);

  return gtk_widget_get_visible (GTK_WIDGET (self->search_bin));
}

void
ide_tweaks_panel_list_set_search_mode (IdeTweaksPanelList *self,
                                       gboolean            search_mode)
{
  g_return_if_fail (IDE_IS_TWEAKS_PANEL_LIST (self));

  search_mode = !!search_mode;

  if (ide_tweaks_panel_list_get_search_mode (self) != search_mode)
    {
      gtk_widget_set_visible (GTK_WIDGET (self->search_bin), search_mode);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SEARCH_MODE]);
    }
}

static void
ide_tweaks_panel_list_select (IdeTweaksPanelList *self,
                              IdeTweaksItem      *item)
{
  g_assert (IDE_IS_TWEAKS_PANEL_LIST (self));
  g_assert (!item || IDE_IS_TWEAKS_ITEM (item));

  if (item == NULL)
    return;

  g_signal_handlers_block_by_func (self->list_box,
                                   G_CALLBACK (ide_tweaks_panel_list_row_activated_cb),
                                   self);

  for (GtkWidget *child = gtk_widget_get_first_child (GTK_WIDGET (self->list_box));
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      IdeTweaksItem *row_item;

      if (!IDE_IS_TWEAKS_PANEL_LIST_ROW (child))
        continue;

      row_item = ide_tweaks_panel_list_row_get_item (IDE_TWEAKS_PANEL_LIST_ROW (child));

      if (row_item == item)
        {
          gtk_list_box_select_row (self->list_box, GTK_LIST_BOX_ROW (child));
          break;
        }
    }

  g_signal_handlers_unblock_by_func (self->list_box,
                                     G_CALLBACK (ide_tweaks_panel_list_row_activated_cb),
                                     self);
}

GtkSelectionMode
ide_tweaks_panel_list_get_selection_mode (IdeTweaksPanelList *self)
{
  g_return_val_if_fail (IDE_IS_TWEAKS_PANEL_LIST (self), 0);

  return gtk_list_box_get_selection_mode (self->list_box);
}

void
ide_tweaks_panel_list_set_selection_mode (IdeTweaksPanelList *self,
                                          GtkSelectionMode    selection_mode)
{
  g_return_if_fail (IDE_IS_TWEAKS_PANEL_LIST (self));

  if (selection_mode != ide_tweaks_panel_list_get_selection_mode (self))
    {
      gtk_list_box_set_selection_mode (self->list_box, selection_mode);

      if (selection_mode == GTK_SELECTION_NONE)
        gtk_list_box_unselect_all (self->list_box);
      else
        ide_tweaks_panel_list_select (self, self->selected);

      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SELECTION_MODE]);
    }
}
