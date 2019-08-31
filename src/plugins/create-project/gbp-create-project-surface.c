/* gbp-create-project-surface.c
 *
 * Copyright 2016-2019 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "gbp-create-project-surface"

#include <dazzle.h>
#include <glib/gi18n.h>
#include <libide-greeter.h>
#include <libide-projects.h>
#include <libide-vcs.h>
#include <libpeas/peas.h>
#include <stdlib.h>
#include <string.h>

#include "ide-greeter-private.h"

#include "gbp-create-project-template-icon.h"
#include "gbp-create-project-surface.h"

struct _GbpCreateProjectSurface
{
  IdeSurface            parent;

  PeasExtensionSet     *providers;

  GtkEntry             *app_id_entry;
  GtkEntry             *project_name_entry;
  DzlFileChooserEntry  *project_location_entry;
  DzlRadioBox          *project_language_chooser;
  GtkFlowBox           *project_template_chooser;
  GtkSwitch            *versioning_switch;
  DzlRadioBox          *license_chooser;
  GtkLabel             *destination_label;
  GtkButton            *create_button;

  guint                 invalid_directory : 1;
};

enum {
  PROP_0,
  PROP_IS_READY,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

G_DEFINE_TYPE (GbpCreateProjectSurface, gbp_create_project_surface, IDE_TYPE_SURFACE)

static gboolean
is_preferred (const gchar *name)
{
  return 0 == strcasecmp (name, "c") ||
         0 == strcasecmp (name, "rust") ||
         0 == strcasecmp (name, "javascript") ||
         0 == strcasecmp (name, "python");
}

static int
sort_by_name (gconstpointer a,
              gconstpointer b)
{
  const gchar * const *astr = a;
  const gchar * const *bstr = b;
  gboolean apref = is_preferred (*astr);
  gboolean bpref = is_preferred (*bstr);

  if (apref && !bpref)
    return -1;
  else if (!apref && bpref)
    return 1;

  return g_utf8_collate (*astr, *bstr);
}

static void
gbp_create_project_surface_add_languages (GbpCreateProjectSurface *self,
                                         const GList            *templates)
{
  g_autoptr(GHashTable) languages = NULL;
  g_autofree const gchar **keys = NULL;
  const GList *iter;
  guint len;

  g_assert (GBP_IS_CREATE_PROJECT_SURFACE (self));

  languages = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  for (iter = templates; iter != NULL; iter = iter->next)
    {
      IdeProjectTemplate *template = iter->data;
      g_auto(GStrv) template_languages = NULL;

      g_assert (IDE_IS_PROJECT_TEMPLATE (template));

      template_languages = ide_project_template_get_languages (template);

      for (guint i = 0; template_languages [i]; i++)
        g_hash_table_add (languages, g_strdup (template_languages [i]));
    }

  keys = (const gchar **)g_hash_table_get_keys_as_array (languages, &len);
  qsort (keys, len, sizeof (gchar *), sort_by_name);
  for (guint i = 0; keys[i]; i++)
    dzl_radio_box_add_item (self->project_language_chooser, keys[i], keys[i]);
}

static gboolean
validate_name (const gchar *name)
{
  if (name == NULL)
    return FALSE;

  if (g_unichar_isdigit (g_utf8_get_char (name)))
    return FALSE;

  for (; *name; name = g_utf8_next_char (name))
    {
      gunichar ch = g_utf8_get_char (name);

      if (g_unichar_isspace (ch))
        return FALSE;

      if (ch == '/')
        return FALSE;
    }

  return TRUE;
}

static gboolean
directory_exists (GbpCreateProjectSurface *self,
                  const gchar            *name)
{
  g_autoptr(GFile) directory = NULL;
  g_autoptr(GFile) child = NULL;

  g_assert (GBP_IS_CREATE_PROJECT_SURFACE (self));
  g_assert (name != NULL);

  directory = dzl_file_chooser_entry_get_file (self->project_location_entry);
  child = g_file_get_child (directory, name);

  self->invalid_directory = g_file_query_exists (child, NULL);

  return self->invalid_directory;
}

static void
gbp_create_project_surface_create_cb (GObject      *object,
                                      GAsyncResult *result,
                                      gpointer      user_data)
{
  GbpCreateProjectSurface *self = (GbpCreateProjectSurface *)object;
  g_autoptr(GError) error = NULL;

  g_assert (GBP_IS_CREATE_PROJECT_SURFACE (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (user_data == NULL);

  if (!gbp_create_project_surface_create_finish (self, result, &error))
    {
      g_warning ("Failed to create project: %s", error->message);
    }
}

static void
gbp_create_project_surface_create_clicked (GbpCreateProjectSurface *self,
                                           GtkButton               *button)
{
  GCancellable *cancellable;
  GtkWidget *workspace;

  g_assert (GBP_IS_CREATE_PROJECT_SURFACE (self));
  g_assert (GTK_IS_BUTTON (button));

  workspace = gtk_widget_get_ancestor (GTK_WIDGET (self), IDE_TYPE_WORKSPACE);
  cancellable = ide_workspace_get_cancellable (IDE_WORKSPACE (workspace));

  gbp_create_project_surface_create_async (self,
                                           cancellable,
                                           gbp_create_project_surface_create_cb,
                                           NULL);
}

static void
gbp_create_project_surface_name_changed (GbpCreateProjectSurface *self,
                                        GtkEntry               *entry)
{
  g_autofree gchar *project_name = NULL;
  const gchar *text;

  g_assert (GBP_IS_CREATE_PROJECT_SURFACE (self));
  g_assert (GTK_IS_ENTRY (entry));

  text = gtk_entry_get_text (entry);
  project_name = g_strstrip (g_strdup (text));

  if (ide_str_empty0 (project_name) || !validate_name (project_name))
    {
      g_object_set (self->project_name_entry,
                    "secondary-icon-name", "dialog-warning-symbolic",
                    "tooltip-text", _("Characters were used which might cause technical issues as a project name"),
                    NULL);
      gtk_label_set_label (self->destination_label,
                           _("Your project will be created within a new child directory."));
    }
  else if (directory_exists (self, project_name))
    {
      g_object_set (self->project_name_entry,
                    "secondary-icon-name", "dialog-warning-symbolic",
                    "tooltip-text", _("Directory already exists with that name"),
                    NULL);
      gtk_label_set_label (self->destination_label, NULL);
    }
  else
    {
      g_autofree gchar *formatted = NULL;
      g_autoptr(GFile) file = dzl_file_chooser_entry_get_file (self->project_location_entry);
      g_autoptr(GFile) child = g_file_get_child (file, project_name);
      g_autofree gchar *path = g_file_get_path (child);
      g_autofree gchar *collapsed = ide_path_collapse (path);

      g_object_set (self->project_name_entry,
                    "secondary-icon-name", NULL,
                    "tooltip-text", NULL,
                    NULL);

      /* translators: %s is replaced with a short-form file-system path to the project */
      formatted = g_strdup_printf (_("Your project will be created within %s."), collapsed);
      gtk_label_set_label (self->destination_label, formatted);
    }

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_IS_READY]);
}

static guint
count_chars (const gchar *str,
             gunichar     ch)
{
  guint count = 0;
  for (; *str; str = g_utf8_next_char (str))
    count += *str == ch;
  return count;
}

static void
gbp_create_project_surface_app_id_changed (GbpCreateProjectSurface *self,
                                          GtkEntry               *entry)
{
  const gchar *app_id;

  g_assert (GBP_IS_CREATE_PROJECT_SURFACE (self));
  g_assert (GTK_IS_ENTRY (entry));

  app_id = gtk_entry_get_text (entry);

  if (!(ide_str_empty0 (app_id) ||
        (g_application_id_is_valid (app_id) && count_chars (app_id, '.') >= 2)))
    {
      g_object_set (self->app_id_entry,
                    "secondary-icon-name", "dialog-warning-symbolic",
                    "tooltip-text", _("Application ID is not valid."),
                    NULL);
    }
  else
    {
      g_object_set (self->app_id_entry,
                    "secondary-icon-name", NULL,
                    "tooltip-text", NULL,
                    NULL);
    }

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_IS_READY]);
}

static void
gbp_create_project_surface_location_changed (GbpCreateProjectSurface *self,
                                            GParamSpec             *pspec,
                                            DzlFileChooserEntry    *chooser)
{
  g_assert (GBP_IS_CREATE_PROJECT_SURFACE (self));
  g_assert (DZL_IS_FILE_CHOOSER_ENTRY (chooser));

  /* Piggyback on the name changed signal to update things */
  gbp_create_project_surface_name_changed (self, self->project_name_entry);
}

static void
update_language_sensitivity (GtkWidget *widget,
                             gpointer   data)
{
  GbpCreateProjectSurface *self = data;
  GbpCreateProjectTemplateIcon *template_icon;
  IdeProjectTemplate *template;
  g_auto(GStrv) template_languages = NULL;
  const gchar *language;
  gboolean sensitive = FALSE;
  gint i;

  g_assert (GBP_IS_CREATE_PROJECT_SURFACE (self));
  g_assert (GTK_IS_FLOW_BOX_CHILD (widget));

  language = dzl_radio_box_get_active_id (self->project_language_chooser);

  if (ide_str_empty0 (language))
    goto apply;

  template_icon = GBP_CREATE_PROJECT_TEMPLATE_ICON (gtk_bin_get_child (GTK_BIN (widget)));
  g_object_get (template_icon,
                "template", &template,
                NULL);
  template_languages = ide_project_template_get_languages (template);

  for (i = 0; template_languages [i]; i++)
    {
      if (g_str_equal (language, template_languages [i]))
        {
          sensitive = TRUE;
          goto apply;
        }
    }

apply:
  gtk_widget_set_sensitive (widget, sensitive);
}

static void
gbp_create_project_surface_refilter (GbpCreateProjectSurface *self)
{
  gtk_container_foreach (GTK_CONTAINER (self->project_template_chooser),
                         update_language_sensitivity,
                         self);
}

static void
gbp_create_project_surface_language_changed (GbpCreateProjectSurface *self,
                                            DzlRadioBox            *language_chooser)
{
  g_assert (GBP_IS_CREATE_PROJECT_SURFACE (self));
  g_assert (DZL_IS_RADIO_BOX (language_chooser));

  gbp_create_project_surface_refilter (self);

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_IS_READY]);
}

static void
gbp_create_project_surface_template_selected (GbpCreateProjectSurface *self,
                                             GtkFlowBox             *box,
                                             GtkFlowBoxChild        *child)
{
  g_assert (GBP_IS_CREATE_PROJECT_SURFACE (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_IS_READY]);
}

static gint
project_template_sort_func (GtkFlowBoxChild *child1,
                            GtkFlowBoxChild *child2,
                            gpointer         user_data)
{
  GbpCreateProjectTemplateIcon *icon1;
  GbpCreateProjectTemplateIcon *icon2;
  IdeProjectTemplate *tmpl1;
  IdeProjectTemplate *tmpl2;

  icon1 = GBP_CREATE_PROJECT_TEMPLATE_ICON (gtk_bin_get_child (GTK_BIN (child1)));
  icon2 = GBP_CREATE_PROJECT_TEMPLATE_ICON (gtk_bin_get_child (GTK_BIN (child2)));

  tmpl1 = gbp_create_project_template_icon_get_template (icon1);
  tmpl2 = gbp_create_project_template_icon_get_template (icon2);

  return ide_project_template_compare (tmpl1, tmpl2);
}

static void
gbp_create_project_surface_add_template_buttons (GbpCreateProjectSurface *self,
                                                 GList                   *templates)
{
  g_assert (GBP_IS_CREATE_PROJECT_SURFACE (self));

  for (const GList *iter = templates; iter; iter = iter->next)
    {
      IdeProjectTemplate *template = iter->data;
      GbpCreateProjectTemplateIcon *template_icon;
      GtkFlowBoxChild *template_container;

      g_assert (IDE_IS_PROJECT_TEMPLATE (template));

      template_icon = g_object_new (GBP_TYPE_CREATE_PROJECT_TEMPLATE_ICON,
                                    "visible", TRUE,
                                    "template", template,
                                    NULL);

      template_container = g_object_new (GTK_TYPE_FLOW_BOX_CHILD,
                                         "visible", TRUE,
                                         NULL);
      gtk_container_add (GTK_CONTAINER (template_container), GTK_WIDGET (template_icon));
      gtk_flow_box_insert (self->project_template_chooser, GTK_WIDGET (template_container), -1);
    }
}

static void
gbp_create_project_surface_provider_added_cb (PeasExtensionSet *set,
                                              PeasPluginInfo   *plugin_info,
                                              PeasExtension    *exten,
                                              gpointer          user_data)
{
  GbpCreateProjectSurface *self = user_data;
  IdeTemplateProvider *provider = (IdeTemplateProvider *)exten;
  g_autolist(IdeProjectTemplate) templates = NULL;
  GtkFlowBoxChild *child;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (GBP_IS_CREATE_PROJECT_SURFACE (self));
  g_assert (IDE_IS_TEMPLATE_PROVIDER (provider));

  templates = ide_template_provider_get_project_templates (provider);

  gbp_create_project_surface_add_template_buttons (self, templates);
  gbp_create_project_surface_add_languages (self, templates);

  gtk_flow_box_invalidate_sort (self->project_template_chooser);
  gbp_create_project_surface_refilter (self);

  /*
   * We do the following after every add, because we might get some delayed
   * additions for templates during startup.
   */

  /* Default to C, always. We might investigate setting this to the
   * previously selected item in the future.
   */
  dzl_radio_box_set_active_id (self->project_language_chooser, "C");

  /* Select the first template that is visible so we have a selection
   * initially without the user having to select. We might also try to
   * re-select a previous item in the future.
   */
  if ((child = gtk_flow_box_get_child_at_index (self->project_template_chooser, 0)))
    gtk_flow_box_select_child (self->project_template_chooser, child);
}

static GFile *
gbp_create_project_surface_get_directory (GbpCreateProjectSurface *self)
{
  g_assert (GBP_IS_CREATE_PROJECT_SURFACE (self));

  return dzl_file_chooser_entry_get_file (self->project_location_entry);
}

static void
gbp_create_project_surface_set_directory (GbpCreateProjectSurface *self,
                                         GFile                  *directory)
{
  g_assert (GBP_IS_CREATE_PROJECT_SURFACE (self));
  g_assert (G_IS_FILE (directory));

  dzl_file_chooser_entry_set_file (self->project_location_entry, directory);
}

static void
gbp_create_project_surface_constructed (GObject *object)
{
  GbpCreateProjectSurface *self = GBP_CREATE_PROJECT_SURFACE (object);

  g_assert (GBP_IS_CREATE_PROJECT_SURFACE (self));

  G_OBJECT_CLASS (gbp_create_project_surface_parent_class)->constructed (object);

  self->providers = peas_extension_set_new (peas_engine_get_default (),
                                            IDE_TYPE_TEMPLATE_PROVIDER,
                                            NULL);

  g_signal_connect (self->providers,
                    "extension-added",
                    G_CALLBACK (gbp_create_project_surface_provider_added_cb),
                    self);

  peas_extension_set_foreach (self->providers,
                              gbp_create_project_surface_provider_added_cb,
                              self);
}

static void
gbp_create_project_surface_destroy (GtkWidget *widget)
{
  GbpCreateProjectSurface *self = (GbpCreateProjectSurface *)widget;

  g_clear_object (&self->providers);

  GTK_WIDGET_CLASS (gbp_create_project_surface_parent_class)->destroy (widget);
}

static gboolean
gbp_create_project_surface_is_ready (GbpCreateProjectSurface *self)
{
  const gchar *text;
  g_autofree gchar *project_name = NULL;
  const gchar *app_id;
  const gchar *language = NULL;
  GList *selected_template = NULL;
  gboolean ret = FALSE;

  g_assert (GBP_IS_CREATE_PROJECT_SURFACE (self));

  if (self->invalid_directory)
    return FALSE;

  text = gtk_entry_get_text (self->project_name_entry);
  project_name = g_strstrip (g_strdup (text));

  if (ide_str_empty0 (project_name) || !validate_name (project_name))
    return FALSE;

  app_id = gtk_entry_get_text (self->app_id_entry);

  if (!(ide_str_empty0 (app_id) || g_application_id_is_valid (app_id)))
    return FALSE;

  language = dzl_radio_box_get_active_id (self->project_language_chooser);

  if (ide_str_empty0 (language))
    return FALSE;

  selected_template = gtk_flow_box_get_selected_children (self->project_template_chooser);

  if (selected_template == NULL)
    return FALSE;

  ret = gtk_widget_get_sensitive (selected_template->data);

  g_list_free (selected_template);

  return ret;
}

static void
gbp_create_project_surface_grab_focus (GtkWidget *widget)
{
  gtk_widget_grab_focus (GTK_WIDGET (GBP_CREATE_PROJECT_SURFACE (widget)->project_name_entry));
}

static void
gbp_create_project_surface_get_property (GObject    *object,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  GbpCreateProjectSurface *self = GBP_CREATE_PROJECT_SURFACE(object);

  switch (prop_id)
    {
    case PROP_IS_READY:
      g_value_set_boolean (value, gbp_create_project_surface_is_ready (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
gbp_create_project_surface_class_init (GbpCreateProjectSurfaceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = gbp_create_project_surface_constructed;
  object_class->get_property = gbp_create_project_surface_get_property;

  widget_class->destroy = gbp_create_project_surface_destroy;
  widget_class->grab_focus = gbp_create_project_surface_grab_focus;

  properties [PROP_IS_READY] =
    g_param_spec_boolean ("is-ready",
                          "Is Ready",
                          "Is Ready",
                          FALSE,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_css_name (widget_class, "createprojectsurface");
  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/create-project/gbp-create-project-surface.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpCreateProjectSurface, app_id_entry);
  gtk_widget_class_bind_template_child (widget_class, GbpCreateProjectSurface, create_button);
  gtk_widget_class_bind_template_child (widget_class, GbpCreateProjectSurface, destination_label);
  gtk_widget_class_bind_template_child (widget_class, GbpCreateProjectSurface, license_chooser);
  gtk_widget_class_bind_template_child (widget_class, GbpCreateProjectSurface, project_language_chooser);
  gtk_widget_class_bind_template_child (widget_class, GbpCreateProjectSurface, project_location_entry);
  gtk_widget_class_bind_template_child (widget_class, GbpCreateProjectSurface, project_name_entry);
  gtk_widget_class_bind_template_child (widget_class, GbpCreateProjectSurface, project_template_chooser);
  gtk_widget_class_bind_template_child (widget_class, GbpCreateProjectSurface, versioning_switch);
}

static void
gbp_create_project_surface_init (GbpCreateProjectSurface *self)
{
  g_autoptr(GFile) projects_dir = NULL;

  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_widget_set_name (GTK_WIDGET (self), "create-project");
  ide_surface_set_title (IDE_SURFACE (self), C_("title", "Start New Project"));

  projects_dir = g_file_new_for_path (ide_get_projects_dir ());
  gbp_create_project_surface_set_directory (self, projects_dir);

  g_signal_connect_object (self->project_name_entry,
                           "changed",
                           G_CALLBACK (gbp_create_project_surface_name_changed),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->app_id_entry,
                           "changed",
                           G_CALLBACK (gbp_create_project_surface_app_id_changed),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->project_location_entry,
                           "notify::file",
                           G_CALLBACK (gbp_create_project_surface_location_changed),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->project_language_chooser,
                           "changed",
                           G_CALLBACK (gbp_create_project_surface_language_changed),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->project_template_chooser,
                           "child-activated",
                           G_CALLBACK (gbp_create_project_surface_template_selected),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->create_button,
                           "clicked",
                           G_CALLBACK (gbp_create_project_surface_create_clicked),
                           self,
                           G_CONNECT_SWAPPED);

  gtk_flow_box_set_sort_func (self->project_template_chooser,
                              project_template_sort_func,
                              NULL, NULL);

  g_object_bind_property (self, "is-ready", self->create_button, "sensitive",
                          G_BINDING_SYNC_CREATE);
}

static void
init_vcs_cb (GObject      *object,
             GAsyncResult *result,
             gpointer      user_data)
{
  IdeVcsInitializer *vcs = (IdeVcsInitializer *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(IdeProjectInfo) project_info = NULL;
  g_autoptr(GError) error = NULL;
  GbpCreateProjectSurface *self;
  GtkWidget *workspace;
  GFile *project_file;

  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!ide_vcs_initializer_initialize_finish (vcs, result, &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      goto cleanup;
    }

  self = ide_task_get_source_object (task);
  g_assert (GBP_IS_CREATE_PROJECT_SURFACE (self));

  project_file = ide_task_get_task_data (task);

  project_info = ide_project_info_new ();
  ide_project_info_set_file (project_info, project_file);
  ide_project_info_set_directory (project_info, project_file);

  workspace = gtk_widget_get_ancestor (GTK_WIDGET (self), IDE_TYPE_GREETER_WORKSPACE);
  ide_greeter_workspace_open_project (IDE_GREETER_WORKSPACE (workspace), project_info);

  ide_task_return_boolean (task, TRUE);

cleanup:
  ide_object_destroy (IDE_OBJECT (vcs));
}

static void
extract_cb (GObject      *object,
            GAsyncResult *result,
            gpointer      user_data)
{
  IdeProjectTemplate *template = (IdeProjectTemplate *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(IdeVcsInitializer) vcs = NULL;
  g_autoptr(GError) error = NULL;
  GbpCreateProjectSurface *self;
  PeasPluginInfo *plugin_info;
  PeasEngine *engine;
  IdeContext *context;
  GFile *project_file;

  /* To keep the UI simple, we only support git from
   * the creation today. However, at the time of writing
   * that is our only supported VCS anyway. If you'd like to
   * add support for an additional VCS, we need to redesign
   * this part of the UI.
   */
  const gchar *vcs_id = "git";

  g_assert (IDE_IS_PROJECT_TEMPLATE (template));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!ide_project_template_expand_finish (template, result, &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  self = ide_task_get_source_object (task);
  g_assert (GBP_IS_CREATE_PROJECT_SURFACE (self));

  project_file = ide_task_get_task_data (task);
  g_assert (G_IS_FILE (project_file));

  if (!gtk_switch_get_active (self->versioning_switch))
    {
      g_autoptr(IdeProjectInfo) project_info = NULL;
      GtkWidget *workspace;

      project_info = ide_project_info_new ();
      ide_project_info_set_file (project_info, project_file);
      ide_project_info_set_directory (project_info, project_file);

      workspace = gtk_widget_get_ancestor (GTK_WIDGET (self), IDE_TYPE_GREETER_WORKSPACE);
      ide_greeter_workspace_open_project (IDE_GREETER_WORKSPACE (workspace), project_info);

      ide_task_return_boolean (task, TRUE);
      return;
    }

  engine = peas_engine_get_default ();
  plugin_info = peas_engine_get_plugin_info (engine, vcs_id);
  if (plugin_info == NULL)
    IDE_GOTO (failure);

  context = ide_widget_get_context (GTK_WIDGET (self));
  vcs = (IdeVcsInitializer *)peas_engine_create_extension (engine, plugin_info,
                                                           IDE_TYPE_VCS_INITIALIZER,
                                                           "parent", context,
                                                           NULL);
  if (vcs == NULL)
    IDE_GOTO (failure);

  ide_vcs_initializer_initialize_async (vcs,
                                        project_file,
                                        ide_task_get_cancellable (task),
                                        init_vcs_cb,
                                        g_object_ref (task));

  return;

failure:
  ide_task_return_new_error (task,
                             G_IO_ERROR,
                             G_IO_ERROR_FAILED,
                             _("A failure occurred while initializing version control"));
}

void
gbp_create_project_surface_create_async (GbpCreateProjectSurface *self,
                                         GCancellable            *cancellable,
                                         GAsyncReadyCallback      callback,
                                         gpointer                 user_data)
{
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GHashTable) params = NULL;
  g_autoptr(IdeProjectTemplate) template = NULL;
  g_autoptr(IdeVcsConfig) vcs_conf = NULL;
  GValue str = G_VALUE_INIT;
  g_autofree gchar *name = NULL;
  g_autofree gchar *path = NULL;
  g_autoptr(GFile) location = NULL;
  g_autoptr(GFile) child = NULL;
  const gchar *language = NULL;
  const gchar *license_id = NULL;
  GtkFlowBoxChild *template_container;
  GbpCreateProjectTemplateIcon *template_icon;
  PeasEngine *engine;
  PeasPluginInfo *plugin_info;
  const gchar *text;
  const gchar *app_id;
  const gchar *vcs_id = "git";
  const gchar *author_name;
  GList *selected_box_child;

  g_return_if_fail (GBP_CREATE_PROJECT_SURFACE (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  gtk_widget_set_sensitive (GTK_WIDGET (self->create_button), FALSE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->license_chooser), FALSE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->project_language_chooser), FALSE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->project_location_entry), FALSE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->project_name_entry), FALSE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->project_template_chooser), FALSE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->versioning_switch), FALSE);

  selected_box_child = gtk_flow_box_get_selected_children (self->project_template_chooser);
  template_container = selected_box_child->data;
  template_icon = GBP_CREATE_PROJECT_TEMPLATE_ICON (gtk_bin_get_child (GTK_BIN (template_container)));
  g_object_get (template_icon,
                "template", &template,
                NULL);
  g_list_free (selected_box_child);

  params = g_hash_table_new_full (g_str_hash,
                                  g_str_equal,
                                  g_free,
                                  (GDestroyNotify)g_variant_unref);

  text = gtk_entry_get_text (self->project_name_entry);
  name = g_strstrip (g_strdup (text));
  g_hash_table_insert (params,
                       g_strdup ("name"),
                       g_variant_ref_sink (g_variant_new_string (g_strdelimit (name, " ", '-'))));

  location = gbp_create_project_surface_get_directory (self);
  child = g_file_get_child (location, name);
  path = g_file_get_path (child);

  g_hash_table_insert (params,
                       g_strdup ("path"),
                       g_variant_ref_sink (g_variant_new_string (path)));

  language = dzl_radio_box_get_active_id (self->project_language_chooser);
  g_hash_table_insert (params,
                       g_strdup ("language"),
                       g_variant_ref_sink (g_variant_new_string (language)));

  license_id = dzl_radio_box_get_active_id (DZL_RADIO_BOX (self->license_chooser));

  if (!g_str_equal (license_id, "none"))
    {
      g_autofree gchar *license_full_path = NULL;
      g_autofree gchar *license_short_path = NULL;

      license_full_path = g_strjoin (NULL, "resource://", "/plugins/create-project/license/full/", license_id, NULL);
      license_short_path = g_strjoin (NULL, "resource://", "/plugins/create-project/license/short/", license_id, NULL);

      g_hash_table_insert (params,
                           g_strdup ("license_full"),
                           g_variant_ref_sink (g_variant_new_string (license_full_path)));

      g_hash_table_insert (params,
                           g_strdup ("license_short"),
                           g_variant_ref_sink (g_variant_new_string (license_short_path)));
    }

  if (gtk_switch_get_active (self->versioning_switch))
    {
      g_hash_table_insert (params,
                           g_strdup ("versioning"),
                           g_variant_ref_sink (g_variant_new_string ("git")));

      engine = peas_engine_get_default ();
      plugin_info = peas_engine_get_plugin_info (engine, vcs_id);

      if (plugin_info != NULL)
        {
          IdeContext *context;

          context = ide_widget_get_context (GTK_WIDGET (self));
          vcs_conf = (IdeVcsConfig *)peas_engine_create_extension (engine, plugin_info,
                                                                   IDE_TYPE_VCS_CONFIG,
                                                                   "parent", context,
                                                                   NULL);

          if (vcs_conf != NULL)
            {
              g_value_init (&str, G_TYPE_STRING);
              ide_vcs_config_get_config (vcs_conf, IDE_VCS_CONFIG_FULL_NAME, &str);
            }
        }
    }

  if (G_VALUE_HOLDS_STRING (&str) && !ide_str_empty0 (g_value_get_string (&str)))
    author_name = g_value_get_string (&str);
  else
    author_name = g_get_real_name ();

  app_id = gtk_entry_get_text (self->app_id_entry);

  if (ide_str_empty0 (app_id))
    app_id = "org.example.App";

  g_hash_table_insert (params,
                       g_strdup ("author"),
                       g_variant_take_ref (g_variant_new_string (author_name)));

  g_hash_table_insert (params,
                       g_strdup ("app-id"),
                       g_variant_take_ref (g_variant_new_string (app_id)));

  g_value_unset (&str);

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_task_data (task, g_file_new_for_path (path), g_object_unref);

  ide_project_template_expand_async (template,
                                     params,
                                     NULL,
                                     extract_cb,
                                     g_object_ref (task));
}

gboolean
gbp_create_project_surface_create_finish (GbpCreateProjectSurface  *self,
                                          GAsyncResult             *result,
                                          GError                  **error)
{
  g_return_val_if_fail (GBP_IS_CREATE_PROJECT_SURFACE (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  gtk_widget_set_sensitive (GTK_WIDGET (self->create_button), TRUE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->license_chooser), TRUE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->project_language_chooser), TRUE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->project_location_entry), TRUE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->project_name_entry), TRUE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->project_template_chooser), TRUE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->versioning_switch), TRUE);

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}
