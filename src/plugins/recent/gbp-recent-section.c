/* gbp-recent-section.c
 *
 * Copyright (C) 2017 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-recent-section"

#include <ide.h>

#include "gbp-recent-project-row.h"
#include "gbp-recent-section.h"

struct _GbpRecentSection
{
  GtkBin      parent_instance;

  GtkListBox *listbox;

  guint       selection_mode : 1;
};

enum {
  PROP_0,
  PROP_HAS_SELECTION,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
gbp_recent_section_get_has_selection_cb (GtkWidget *widget,
                                         gpointer   user_data)
{
  gboolean *has_selection = user_data;
  gboolean selected = FALSE;

  g_object_get (widget, "selected", &selected, NULL);
  *has_selection |= selected;
}

static gboolean
gbp_recent_section_get_has_selection (GbpRecentSection *self)
{
  gboolean has_selection = FALSE;

  g_assert (GBP_IS_RECENT_SECTION (self));

  gtk_container_foreach (GTK_CONTAINER (self->listbox),
                         gbp_recent_section_get_has_selection_cb,
                         &has_selection);

  return has_selection;
}

static gint
gbp_recent_section_get_priority (IdeGreeterSection *section)
{
  return 0;
}

static void
gbp_recent_section_filter_cb (GtkListBoxRow *row,
                              gpointer       user_data)
{
  struct {
    DzlPatternSpec *spec;
    gboolean found;
  } *filter = user_data;
  gboolean match;

  g_assert (GBP_IS_RECENT_PROJECT_ROW (row));
  g_assert (filter != NULL);

  if (filter->spec != NULL)
    {
      const gchar *search_text;

      search_text = gbp_recent_project_row_get_search_text (GBP_RECENT_PROJECT_ROW (row));
      match = dzl_pattern_spec_match (filter->spec, search_text);
    }

  gtk_widget_set_visible (GTK_WIDGET (row), match);

  filter->found |= match;
}

static gboolean
gbp_recent_section_filter (IdeGreeterSection *section,
                           DzlPatternSpec    *spec)
{
  GbpRecentSection *self = (GbpRecentSection *)section;
  struct {
    DzlPatternSpec *spec;
    gboolean found;
  } filter = { spec, FALSE };

  g_assert (GBP_IS_RECENT_SECTION (self));

  /* We don't use filter func here so that we know if any
   * rows matched. We have to hide our widget if there are
   * no visible matches.
   */

  gtk_container_foreach (GTK_CONTAINER (self->listbox),
                         (GtkCallback) gbp_recent_section_filter_cb,
                         &filter);

  return filter.found;
}

static void
gbp_recent_section_activate_first_cb (GtkWidget *widget,
                                      gpointer   user_data)
{
  IdeProjectInfo *project_info;
  struct {
    GbpRecentSection *self;
    gboolean handled;
  } *activate = user_data;

  g_assert (GBP_IS_RECENT_PROJECT_ROW (widget));
  g_assert (activate != NULL);

  if (activate->handled || !gtk_widget_get_visible (widget))
    return;

  project_info = gbp_recent_project_row_get_project_info (GBP_RECENT_PROJECT_ROW (widget));
  ide_greeter_section_emit_project_activated (IDE_GREETER_SECTION (activate->self), project_info);

  activate->handled = TRUE;
}

static gboolean
gbp_recent_section_activate_first (IdeGreeterSection *section)
{
  GbpRecentSection *self = (GbpRecentSection *)section;
  struct {
    GbpRecentSection *self;
    gboolean handled;
  } activate;

  g_return_val_if_fail (GBP_IS_RECENT_SECTION (self), FALSE);

  activate.self = self;
  activate.handled = FALSE;

  gtk_container_foreach (GTK_CONTAINER (self->listbox),
                         gbp_recent_section_activate_first_cb,
                         &activate);

  return activate.handled;
}

static void
gbp_recent_section_set_selection_mode_cb (GtkWidget *widget,
                                          gpointer   user_data)
{
  GbpRecentProjectRow *row = (GbpRecentProjectRow *)widget;
  gboolean *selection_mode = user_data;

  g_assert (GBP_IS_RECENT_PROJECT_ROW (row));
  g_assert (selection_mode != NULL);

  gbp_recent_project_row_set_selection_mode (row, *selection_mode);
  g_object_set (row, "selected", FALSE, NULL);
}

static void
gbp_recent_section_set_selection_mode (IdeGreeterSection *section,
                                       gboolean           selection_mode)
{
  GbpRecentSection *self = (GbpRecentSection *)section;

  g_assert (GBP_IS_RECENT_SECTION (self));

  gtk_container_foreach (GTK_CONTAINER (self->listbox),
                         gbp_recent_section_set_selection_mode_cb,
                         &selection_mode);

  self->selection_mode = selection_mode;

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_HAS_SELECTION]);
}

static void
gbp_recent_section_delete_selected_cb (GtkWidget *widget,
                                       gpointer   user_data)
{
  GbpRecentProjectRow *row = (GbpRecentProjectRow *)widget;
  GList **list = user_data;
  gboolean selected = FALSE;

  g_assert (GBP_IS_RECENT_PROJECT_ROW (row));
  g_assert (list != NULL);

  g_object_get (row, "selected", &selected, NULL);

  if (selected)
    {
      IdeProjectInfo *project_info;

      project_info = gbp_recent_project_row_get_project_info (row);
      *list = g_list_prepend (*list, g_object_ref (project_info));
      gtk_widget_destroy (GTK_WIDGET (row));
    }
}

static void
gbp_recent_section_delete_selected (IdeGreeterSection *section)
{
  GbpRecentSection *self = (GbpRecentSection *)section;
  IdeRecentProjects *projects;
  GList *infos = NULL;

  g_assert (GBP_IS_RECENT_SECTION (self));

  gtk_container_foreach (GTK_CONTAINER (self->listbox),
                         gbp_recent_section_delete_selected_cb,
                         &infos);

  projects = ide_application_get_recent_projects (IDE_APPLICATION_DEFAULT);
  ide_recent_projects_remove (projects, infos);
  g_list_free_full (infos, g_object_unref);
}

static void
greeter_section_iface_init (IdeGreeterSectionInterface *iface)
{
  iface->get_priority = gbp_recent_section_get_priority;
  iface->filter = gbp_recent_section_filter;
  iface->activate_first = gbp_recent_section_activate_first;
  iface->set_selection_mode = gbp_recent_section_set_selection_mode;
  iface->delete_selected = gbp_recent_section_delete_selected;
}

G_DEFINE_TYPE_WITH_CODE (GbpRecentSection, gbp_recent_section, GTK_TYPE_BIN,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_GREETER_SECTION,
                                                greeter_section_iface_init))

static void
gbp_recent_section_row_activated (GbpRecentSection    *self,
                                  GbpRecentProjectRow *row,
                                  GtkListBox          *list_box)
{
  g_assert (GBP_IS_RECENT_SECTION (self));
  g_assert (GBP_IS_RECENT_PROJECT_ROW (row));
  g_assert (GTK_IS_LIST_BOX (list_box));

  if (self->selection_mode)
    {
      gboolean selected = FALSE;

      g_object_get (row, "selected", &selected, NULL);
      g_object_set (row, "selected", !selected, NULL);

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_HAS_SELECTION]);
    }
  else
    {
      IdeProjectInfo *project_info;

      project_info = gbp_recent_project_row_get_project_info (row);
      ide_greeter_section_emit_project_activated (IDE_GREETER_SECTION (self), project_info);
    }
}

static GtkWidget *
create_widget_func (gpointer item,
                    gpointer user_data)
{
  IdeProjectInfo *project_info = item;
  GbpRecentProjectRow *row;

  g_assert (IDE_IS_PROJECT_INFO (project_info));

  row = g_object_new (GBP_TYPE_RECENT_PROJECT_ROW,
                      "project-info", project_info,
                      "visible", TRUE,
                      NULL);

  return GTK_WIDGET (row);
}

static void
gbp_recent_section_constructed (GObject *object)
{
  GbpRecentSection *self = (GbpRecentSection *)object;
  IdeRecentProjects *projects;

  G_OBJECT_CLASS (gbp_recent_section_parent_class)->constructed (object);

  projects = ide_application_get_recent_projects (IDE_APPLICATION_DEFAULT);

  gtk_list_box_bind_model (self->listbox,
                           G_LIST_MODEL (projects),
                           create_widget_func, self, NULL);
}

static void
gbp_recent_section_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  GbpRecentSection *self = GBP_RECENT_SECTION (object);

  switch (prop_id)
    {
    case PROP_HAS_SELECTION:
      g_value_set_boolean (value, gbp_recent_section_get_has_selection (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_recent_section_class_init (GbpRecentSectionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = gbp_recent_section_constructed;
  object_class->get_property = gbp_recent_section_get_property;

  properties [PROP_HAS_SELECTION] =
    g_param_spec_boolean ("has-selection", NULL, NULL,
                          FALSE,
                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_css_name (widget_class, "recent");
  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/builder/plugins/recent-plugin/gbp-recent-section.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpRecentSection, listbox);
  gtk_widget_class_bind_template_callback (widget_class, gbp_recent_section_row_activated);

  g_type_ensure (GBP_TYPE_RECENT_PROJECT_ROW);
}

static void
gbp_recent_section_init (GbpRecentSection *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}
