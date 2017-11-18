/* gbp-newcomers-section.c
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

#define G_LOG_DOMAIN "gbp-newcomers-section"

#include <ide.h>

#include "gbp-newcomers-project.h"
#include "gbp-newcomers-section.h"

struct _GbpNewcomersSection
{
  GtkBin      parent_instance;
  GtkFlowBox *flowbox;
};

static void gbp_newcomers_section_child_activated (GbpNewcomersSection *self,
                                                   GbpNewcomersProject *project,
                                                   GtkFlowBox          *flowbox);
static gint
gbp_newcomers_section_get_priority (IdeGreeterSection *section)
{
  return 100;
}

static void
gbp_newcomers_section_filter_child (GtkWidget *child,
                                    gpointer   user_data)
{
  struct {
    DzlPatternSpec *spec;
    gboolean found;
  } *filter = user_data;
  gboolean match = TRUE;

  g_assert (GBP_IS_NEWCOMERS_PROJECT (child));
  g_assert (user_data != NULL);

  if (filter->spec != NULL)
    {
      const gchar *name;

      name = gbp_newcomers_project_get_name (GBP_NEWCOMERS_PROJECT (child));
      match = dzl_pattern_spec_match (filter->spec, name);
    }

  gtk_widget_set_visible (child, match);

  filter->found |= match;
}

static gboolean
gbp_newcomers_section_filter (IdeGreeterSection *section,
                              DzlPatternSpec    *spec)
{
  GbpNewcomersSection *self = (GbpNewcomersSection *)section;
  struct {
    DzlPatternSpec *spec;
    gboolean found;
  } filter = { spec, FALSE };

  g_assert (GBP_IS_NEWCOMERS_SECTION (self));

  gtk_container_foreach (GTK_CONTAINER (self->flowbox),
                         gbp_newcomers_section_filter_child,
                         &filter);

  return filter.found;
}

static void
gbp_newcomers_section_activate_cb (GtkWidget *widget,
                                   gpointer   user_data)
{
  GbpNewcomersProject *project = (GbpNewcomersProject *)widget;
  struct {
    GbpNewcomersSection *self;
    gboolean handled;
  } *activate = user_data;

  g_assert (GBP_IS_NEWCOMERS_PROJECT (project));
  g_assert (activate != NULL);
  g_assert (GBP_IS_NEWCOMERS_SECTION (activate->self));

  if (activate->handled || !gtk_widget_get_visible (widget))
    return;

  gbp_newcomers_section_child_activated (activate->self,
                                         project,
                                         activate->self->flowbox);

  activate->handled = TRUE;
}

static gboolean
gbp_newcomers_section_activate_first (IdeGreeterSection *section)
{
  GbpNewcomersSection *self = (GbpNewcomersSection *)section;
  struct {
    GbpNewcomersSection *self;
    gboolean handled;
  } activate;

  g_assert (GBP_IS_NEWCOMERS_SECTION (self));

  activate.self = self;
  activate.handled = FALSE;

  gtk_container_foreach (GTK_CONTAINER (self->flowbox),
                         gbp_newcomers_section_activate_cb,
                         &activate);

  return activate.handled;
}

static void
greeter_section_iface_init (IdeGreeterSectionInterface *iface)
{
  iface->get_priority = gbp_newcomers_section_get_priority;
  iface->filter = gbp_newcomers_section_filter;
  iface->activate_first = gbp_newcomers_section_activate_first;
}

G_DEFINE_TYPE_WITH_CODE (GbpNewcomersSection, gbp_newcomers_section, GTK_TYPE_BIN,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_GREETER_SECTION,
                                                greeter_section_iface_init))

static void
gbp_newcomers_section_child_activated (GbpNewcomersSection *self,
                                       GbpNewcomersProject *project,
                                       GtkFlowBox          *flowbox)
{
  g_autoptr(IdeProjectInfo) project_info = NULL;
  g_autoptr(IdeVcsUri) vcs_uri = NULL;
  const gchar *name;
  const gchar *uri;

  g_assert (GBP_IS_NEWCOMERS_SECTION (self));
  g_assert (GBP_IS_NEWCOMERS_PROJECT (project));
  g_assert (GTK_IS_FLOW_BOX (flowbox));

  name = gbp_newcomers_project_get_name (project);
  uri = gbp_newcomers_project_get_uri (project);
  vcs_uri = ide_vcs_uri_new (uri);

  project_info = g_object_new (IDE_TYPE_PROJECT_INFO,
                               "vcs-uri", vcs_uri,
                               "name", name,
                               NULL);

  ide_greeter_section_emit_project_activated (IDE_GREETER_SECTION (self), project_info);
}

static void
gbp_newcomers_section_class_init (GbpNewcomersSectionClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_css_name (widget_class, "newcomers");
  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/builder/plugins/newcomers-plugin/gbp-newcomers-section.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpNewcomersSection, flowbox);
  gtk_widget_class_bind_template_callback (widget_class, gbp_newcomers_section_child_activated);

  g_type_ensure (GBP_TYPE_NEWCOMERS_PROJECT);
}

static void
gbp_newcomers_section_init (GbpNewcomersSection *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}
