/* gbp-create-project-widget.c
 *
 * Copyright (C) 2016 Christian Hergert <christian@hergert.me>
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

#include <libpeas/peas.h>
#include <stdlib.h>

#include "ide-macros.h"
#include "gbp-create-project-template-icon.h"
#include "gbp-create-project-widget.h"

struct _GbpCreateProjectWidget
{
  GtkBin                parent;

  GtkEntry             *project_name_entry;
  GtkEntry             *project_location_entry;
  GtkFileChooserButton *project_location_button;
  GtkComboBoxText      *project_language_chooser;
  GtkFlowBox           *project_template_chooser;
};

enum {
  PROP_0,
  PROP_IS_READY,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

G_DEFINE_TYPE (GbpCreateProjectWidget, gbp_create_project_widget, GTK_TYPE_BIN)

static void
gbp_create_project_widget_add_languages (GbpCreateProjectWidget *self,
                                         GList                  *project_templates)
{
  g_autoptr(GHashTable) languages = NULL;
  const GList *iter;
  const gchar **keys;
  guint len;
  guint i;

  g_assert (GBP_IS_CREATE_PROJECT_WIDGET (self));

  languages = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  for (iter = project_templates; iter != NULL; iter = iter->next)
    {
      IdeProjectTemplate *template = iter->data;
      g_auto(GStrv) template_languages = NULL;

      g_assert (IDE_IS_PROJECT_TEMPLATE (template));

      template_languages = ide_project_template_get_languages (template);

      for (i = 0; template_languages [i]; i++)
        g_hash_table_add (languages, g_strdup (template_languages [i]));
    }

  keys = (const gchar **)g_hash_table_get_keys_as_array (languages, &len);
  qsort (keys, len, sizeof (gchar *), (GCompareFunc)g_utf8_collate);
  for (i = 0; keys [i]; i++)
    gtk_combo_box_text_append (self->project_language_chooser, NULL, keys [i]);
  g_free (keys);
}

static gboolean
validate_name (const gchar *name)
{
  for (; *name; name = g_utf8_next_char (name))
    {
      gunichar ch = g_utf8_get_char (name);

      if (ch == '/')
        return FALSE;
    }

  return TRUE;
}

static void
gbp_create_project_widget_name_changed (GbpCreateProjectWidget *self,
                                        GtkEntry               *entry)
{
  const gchar *text;
  g_autofree gchar *project_name = NULL;
  g_autofree gchar *project_dir = NULL;

  g_assert (GBP_IS_CREATE_PROJECT_WIDGET (self));
  g_assert (GTK_IS_ENTRY (entry));

  text = gtk_entry_get_text (entry);
  project_name = g_strstrip (g_strdup (text));

  if (ide_str_empty0 (project_name) || !validate_name (project_name))
    {
      g_object_set (self->project_name_entry,
                    "secondary-icon-name", "dialog-warning-symbolic",
                    NULL);

      gtk_entry_set_text (self->project_location_entry, "");
    }
  else
    {
      g_object_set (self->project_name_entry,
                    "secondary-icon-name", NULL,
                    NULL);

      project_dir = g_ascii_strdown (g_strdelimit (project_name, " ", '-'), -1);
      gtk_entry_set_text (self->project_location_entry, project_dir);
    }

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_IS_READY]);
}

static gboolean
gbp_create_project_widget_flow_box_filter (GtkFlowBoxChild *template_container,
                                           gpointer         object)
{
  GbpCreateProjectWidget *self = object;
  GbpCreateProjectTemplateIcon *template_icon;
  IdeProjectTemplate *template;
  g_autofree gchar *language = NULL;
  g_auto(GStrv) template_languages = NULL;
  gint i;

  g_assert (GTK_IS_FLOW_BOX_CHILD (template_container));
  g_assert (GBP_IS_CREATE_PROJECT_WIDGET (self));

  language = gtk_combo_box_text_get_active_text (self->project_language_chooser);

  if (ide_str_empty0 (language))
    return TRUE;

  template_icon = GBP_CREATE_PROJECT_TEMPLATE_ICON (gtk_bin_get_child (GTK_BIN (template_container)));
  g_object_get (template_icon,
                "template", &template,
                NULL);
  template_languages = ide_project_template_get_languages (template);
  g_object_unref (template);

  for (i = 0; template_languages [i]; i++)
    {
      if (g_str_equal (language, template_languages [i]))
        return TRUE;
    }

  gtk_flow_box_unselect_child (self->project_template_chooser, template_container);

  return FALSE;
}

static void
gbp_create_project_widget_language_changed (GbpCreateProjectWidget *self,
                                            GtkComboBox            *language_chooser)
{
  g_assert (GBP_IS_CREATE_PROJECT_WIDGET (self));

  gtk_flow_box_invalidate_filter (self->project_template_chooser);

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_IS_READY]);
}

static void
gbp_create_project_widget_template_selected (GbpCreateProjectWidget *self,
                                             GtkFlowBox             *box,
                                             GtkFlowBoxChild        *child)
{
  g_assert (GBP_IS_CREATE_PROJECT_WIDGET (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_IS_READY]);
}

static void
gbp_create_project_widget_add_template_buttons (GbpCreateProjectWidget *self,
                                                GList                  *project_templates)
{
  const GList *iter;

  g_assert (GBP_IS_CREATE_PROJECT_WIDGET (self));

  for (iter = project_templates; iter != NULL; iter = iter->next)
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
template_providers_foreach_cb (PeasExtensionSet *set,
                               PeasPluginInfo   *plugin_info,
                               PeasExtension    *exten,
                               gpointer          user_data)
{
  GbpCreateProjectWidget *self = user_data;
  IdeTemplateProvider *provider = IDE_TEMPLATE_PROVIDER (exten);
  GList *templates = ide_template_provider_get_project_templates (provider);

  g_assert (GBP_IS_CREATE_PROJECT_WIDGET (self));

  gbp_create_project_widget_add_template_buttons (self, templates);
  gbp_create_project_widget_add_languages (self, templates);

  g_list_foreach (templates, (GFunc)g_object_unref, NULL);
  g_list_free (templates);
}

static void
gbp_create_project_widget_constructed (GObject *object)
{
  GbpCreateProjectWidget *self = GBP_CREATE_PROJECT_WIDGET (object);
  PeasEngine *engine;
  PeasExtensionSet *extensions;

  engine = peas_engine_get_default ();
  extensions = peas_extension_set_new (engine,
                                       IDE_TYPE_TEMPLATE_PROVIDER,
                                       NULL);
  peas_extension_set_foreach (extensions,
                              template_providers_foreach_cb,
                              self);

  g_clear_object (&extensions);

  G_OBJECT_CLASS (gbp_create_project_widget_parent_class)->constructed (object);

  gtk_combo_box_set_active (GTK_COMBO_BOX (self->project_language_chooser), 0);
}

static void
gbp_create_project_widget_finalize (GObject *object)
{
  G_OBJECT_CLASS (gbp_create_project_widget_parent_class)->finalize (object);
}

static gboolean
gbp_create_project_widget_is_ready (GbpCreateProjectWidget *self)
{
  const gchar *text;
  g_autofree gchar *project_name = NULL;
  g_autofree gchar *language = NULL;
  GList *selected_template = NULL;

  g_assert (GBP_IS_CREATE_PROJECT_WIDGET (self));

  text = gtk_entry_get_text (self->project_name_entry);
  project_name = g_strstrip (g_strdup (text));

  if (ide_str_empty0 (project_name) || !validate_name (project_name))
    return FALSE;

  language = gtk_combo_box_text_get_active_text (self->project_language_chooser);

  if (ide_str_empty0 (language))
    return FALSE;

  selected_template = gtk_flow_box_get_selected_children (self->project_template_chooser);

  if (selected_template == NULL)
    return FALSE;

  g_list_free (selected_template);

  return TRUE;
}

static void
gbp_create_project_widget_get_property (GObject    *object,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  GbpCreateProjectWidget *self = GBP_CREATE_PROJECT_WIDGET(object);

  switch (prop_id)
    {
    case PROP_IS_READY:
      g_value_set_boolean (value, gbp_create_project_widget_is_ready (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
gbp_create_project_widget_class_init (GbpCreateProjectWidgetClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = gbp_create_project_widget_constructed;
  object_class->finalize = gbp_create_project_widget_finalize;
  object_class->get_property = gbp_create_project_widget_get_property;

  properties [PROP_IS_READY] =
    g_param_spec_boolean ("is-ready",
                          "Is Ready",
                          "Is Ready",
                          FALSE,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_css_name (widget_class, "createprojectwidget");
  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/builder/plugins/create-project-plugin/gbp-create-project-widget.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpCreateProjectWidget, project_name_entry);
  gtk_widget_class_bind_template_child (widget_class, GbpCreateProjectWidget, project_location_button);
  gtk_widget_class_bind_template_child (widget_class, GbpCreateProjectWidget, project_location_entry);
  gtk_widget_class_bind_template_child (widget_class, GbpCreateProjectWidget, project_language_chooser);
  gtk_widget_class_bind_template_child (widget_class, GbpCreateProjectWidget, project_template_chooser);
}

static void
gbp_create_project_widget_init (GbpCreateProjectWidget *self)
{
  g_autoptr(GSettings) settings = NULL;
  g_autofree gchar *path = NULL;
  g_autofree char *projects_dir = NULL;

  gtk_widget_init_template (GTK_WIDGET (self));

  settings = g_settings_new ("org.gnome.builder");
  path = g_settings_get_string (settings, "projects-directory");

  if (!ide_str_empty0 (path))
    {
      if (!g_path_is_absolute (path))
        projects_dir = g_build_filename (g_get_home_dir (), path, NULL);
      else
        projects_dir = g_steal_pointer (&path);

      gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (self->project_location_button),
                                           projects_dir);
    }

  gtk_flow_box_set_filter_func (self->project_template_chooser,
                                gbp_create_project_widget_flow_box_filter,
                                self,
                                NULL);

  g_signal_connect_object (self->project_name_entry,
                           "changed",
                           G_CALLBACK (gbp_create_project_widget_name_changed),
                           self,
                           G_CONNECT_SWAPPED);


  g_signal_connect_object (self->project_language_chooser,
                           "changed",
                           G_CALLBACK (gbp_create_project_widget_language_changed),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->project_template_chooser,
                           "child-activated",
                           G_CALLBACK (gbp_create_project_widget_template_selected),
                           self,
                           G_CONNECT_SWAPPED);
}

static void
extract_cb (GObject      *object,
            GAsyncResult *result,
            gpointer      user_data)
{
  GbpCreateProjectWidget *self;
  IdeWorkbench *workbench;
  IdeProjectTemplate *template = (IdeProjectTemplate *)object;
  g_autoptr(GTask) task = user_data;
  GError *error = NULL;
  const gchar *path;
  g_autoptr(GFile) project_file = NULL;

  g_assert (IDE_IS_PROJECT_TEMPLATE (template));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));


  if (!ide_project_template_expand_finish (template, result, &error))
    {
      g_object_unref (template);
      g_task_return_error (task, error);
      return;
    }
  else
    {
      self = g_task_get_source_object (task);
      path = g_task_get_task_data (task);
      project_file = g_file_new_for_path (path);
      workbench = ide_widget_get_workbench (GTK_WIDGET (self));
      ide_workbench_open_project_async (workbench, project_file, NULL, NULL, NULL);
    }

  g_object_unref (template);
  g_task_return_boolean (task, TRUE);
}

void
gbp_create_project_widget_create_async (GbpCreateProjectWidget *self,
                                        GCancellable           *cancellable,
                                        GAsyncReadyCallback     callback,
                                        gpointer                user_data)
{
  g_autoptr(GTask) task = NULL;
  g_autoptr(GHashTable) params = NULL;
  IdeProjectTemplate *template;
  g_autofree gchar *name;
  g_autofree gchar *location = NULL;
  g_autofree gchar *path = NULL;
  g_autofree gchar *language = NULL;
  GtkFlowBoxChild *template_container;
  GbpCreateProjectTemplateIcon *template_icon;
  const gchar *text;
  const gchar *child_name;
  GList *selected_box_child = NULL;

  g_return_if_fail (GBP_CREATE_PROJECT_WIDGET (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

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

  child_name = gtk_entry_get_text (self->project_location_entry);
  location = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (self->project_location_button));

  if (!ide_str_empty0 (child_name))
    path = g_build_filename (location, child_name, NULL);
  else
    path = g_steal_pointer (&location);

  g_hash_table_insert (params,
                       g_strdup ("path"),
                       g_variant_ref_sink (g_variant_new_string (path)));

  language = gtk_combo_box_text_get_active_text (self->project_language_chooser);
  g_hash_table_insert (params,
                       g_strdup ("language"),
                       g_variant_ref_sink (g_variant_new_string (language)));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_task_data (task, g_strdup (path), g_free);

  ide_project_template_expand_async (g_object_ref (template),
                                     params,
                                     NULL,
                                     extract_cb,
                                     g_object_ref (task));
  g_object_unref (template);
}

gboolean
gbp_create_project_widget_create_finish (GbpCreateProjectWidget *self,
                                         GAsyncResult           *result,
                                         GError                **error)
{
  g_return_val_if_fail (GBP_IS_CREATE_PROJECT_WIDGET (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}
