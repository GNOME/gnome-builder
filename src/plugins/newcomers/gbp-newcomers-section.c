/* gbp-newcomers-section.c
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

#define G_LOG_DOMAIN "gbp-newcomers-section"

#include <libide-greeter.h>
#include <libide-vcs.h>

#include "gbp-newcomers-project.h"
#include "gbp-newcomers-section.h"

struct _GbpNewcomersSection
{
  GtkBin      parent_instance;
  GtkListBox *list_box;
};

typedef struct
{
  GbpNewcomersSection *self;
  GbpNewcomersProject *project;
  guint                mode;
} DelayedActivate;

enum {
  PROP_0,
  PROP_HAS_SELECTION,
  N_PROPS
};

static void gbp_newcomers_section_row_activated (GbpNewcomersSection *self,
                                                 GbpNewcomersProject *project,
                                                 GtkListBox          *list_box);

static void
delayed_activate_free (gpointer data)
{
  DelayedActivate *state = data;

  g_clear_object (&state->self);
  g_clear_object (&state->project);
  g_slice_free (DelayedActivate, state);
}

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

  gtk_container_foreach (GTK_CONTAINER (self->list_box),
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

  gbp_newcomers_section_row_activated (activate->self,
                                         project,
                                         activate->self->list_box);

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

  gtk_container_foreach (GTK_CONTAINER (self->list_box),
                         gbp_newcomers_section_activate_cb,
                         &activate);

  return activate.handled;
}

static void
gbp_newcomers_section_set_selection_mode (IdeGreeterSection *section,
                                          gboolean           selection_mode)
{
  gtk_widget_set_visible (GTK_WIDGET (section), !selection_mode);
}

static void
greeter_section_iface_init (IdeGreeterSectionInterface *iface)
{
  iface->get_priority = gbp_newcomers_section_get_priority;
  iface->filter = gbp_newcomers_section_filter;
  iface->activate_first = gbp_newcomers_section_activate_first;
  iface->set_selection_mode = gbp_newcomers_section_set_selection_mode;
}

G_DEFINE_TYPE_WITH_CODE (GbpNewcomersSection, gbp_newcomers_section, GTK_TYPE_BIN,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_GREETER_SECTION,
                                                greeter_section_iface_init))

static gboolean
clear_selection_from_timeout (gpointer data)
{
  GbpNewcomersSection *self = data;

  g_assert (GBP_IS_NEWCOMERS_SECTION (self));

  if (self->list_box != NULL)
    gtk_list_box_selected_foreach (self->list_box,
                                   (GtkListBoxForeachFunc)gtk_list_box_unselect_row,
                                   NULL);

  return G_SOURCE_REMOVE;
}

static gboolean
do_selection_from_timeout (gpointer data)
{
  DelayedActivate *state = data;
  g_autoptr(IdeProjectInfo) project_info = NULL;
  const gchar *name;
  const gchar *uri;

  g_assert (state != NULL);
  g_assert (GBP_IS_NEWCOMERS_SECTION (state->self));
  g_assert (GBP_IS_NEWCOMERS_PROJECT (state->project));

  name = gbp_newcomers_project_get_name (state->project);
  uri = gbp_newcomers_project_get_uri (state->project);

  project_info = g_object_new (IDE_TYPE_PROJECT_INFO,
                               "vcs-uri", uri,
                               "name", name,
                               NULL);

  ide_greeter_section_emit_project_activated (IDE_GREETER_SECTION (state->self),
                                              project_info);

  g_timeout_add_full (G_PRIORITY_HIGH,
                      300,
                      clear_selection_from_timeout,
                      g_object_ref (state->self),
                      g_object_unref);


  return G_SOURCE_REMOVE;
}

static void
gbp_newcomers_section_row_activated (GbpNewcomersSection *self,
                                     GbpNewcomersProject *project,
                                     GtkListBox          *list_box)
{
  DelayedActivate *state;

  g_assert (GBP_IS_NEWCOMERS_SECTION (self));
  g_assert (GBP_IS_NEWCOMERS_PROJECT (project));
  g_assert (GTK_IS_LIST_BOX (list_box));

  state = g_slice_new0 (DelayedActivate);
  state->self = g_object_ref (self);
  state->project = g_object_ref (project);

  /* Delay the selection for just a moment so the user can actually
   * see what selection they made.
   */
  g_timeout_add_full (G_PRIORITY_HIGH,
                      150,
                      do_selection_from_timeout,
                      g_steal_pointer (&state),
                      delayed_activate_free);
}

static void
gbp_newcomers_section_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  switch (prop_id)
    {
    case PROP_HAS_SELECTION:
      g_value_set_boolean (value, FALSE);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_newcomers_section_class_init (GbpNewcomersSectionClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = gbp_newcomers_section_get_property;

  g_object_class_install_property (object_class,
                                   PROP_HAS_SELECTION,
                                   g_param_spec_boolean ("has-selection", NULL, NULL,
                                                         FALSE,
                                                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  gtk_widget_class_set_css_name (widget_class, "newcomers");
  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/newcomers/gbp-newcomers-section.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpNewcomersSection, list_box);

  g_type_ensure (GBP_TYPE_NEWCOMERS_PROJECT);
}

static void
gbp_newcomers_section_init (GbpNewcomersSection *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect_object (self->list_box,
                           "row-activated",
                           G_CALLBACK (gbp_newcomers_section_row_activated),
                           self,
                           G_CONNECT_SWAPPED | G_CONNECT_AFTER);
}
