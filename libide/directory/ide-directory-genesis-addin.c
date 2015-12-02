/* ide-directory-genesis-addin.c
 *
 * Copyright (C) 2015 Christian Hergert <chergert@redhat.com>
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

#include <glib/gi18n.h>
#include <libpeas/peas.h>

#include "ide-directory-genesis-addin.h"
#include "ide-genesis-addin.h"
#include "ide-gtk.h"
#include "ide-workbench.h"

struct _IdeDirectoryGenesisAddin
{
  GObject               parent_instance;
  guint                 is_ready : 1;
  GtkFileChooserWidget *widget;
};

static void genesis_addin_iface_init (IdeGenesisAddinInterface *iface);

G_DEFINE_TYPE_EXTENDED (IdeDirectoryGenesisAddin, ide_directory_genesis_addin, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_GENESIS_ADDIN, genesis_addin_iface_init))

enum {
  PROP_0,
  PROP_IS_READY,
  LAST_PROP
};

static gboolean
ide_directory_genesis_addin_get_is_ready (IdeDirectoryGenesisAddin *self)
{
  g_autoptr(GFile) file = NULL;

  g_assert (IDE_IS_DIRECTORY_GENESIS_ADDIN (self));

  file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (self->widget));

  return (file != NULL);
}

static void
ide_directory_genesis_addin_get_property (GObject    *object,
                                          guint       prop_id,
                                          GValue     *value,
                                          GParamSpec *pspec)
{
  IdeDirectoryGenesisAddin *self = IDE_DIRECTORY_GENESIS_ADDIN(object);

  switch (prop_id)
    {
    case PROP_IS_READY:
      g_value_set_boolean (value, ide_directory_genesis_addin_get_is_ready (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
ide_directory_genesis_addin_class_init (IdeDirectoryGenesisAddinClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = ide_directory_genesis_addin_get_property;

  g_object_class_install_property (object_class,
                                   PROP_IS_READY,
                                   g_param_spec_boolean ("is-ready",
                                                         "Is Ready",
                                                         "Is Ready",
                                                         FALSE,
                                                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS)));
}

static void
ide_directory_genesis_addin_init (IdeDirectoryGenesisAddin *self)
{
}

static gchar *
ide_directory_genesis_addin_get_icon_name (IdeGenesisAddin *addin)
{
  return g_strdup ("folder-symbolic");
}

static gchar *
ide_directory_genesis_addin_get_title (IdeGenesisAddin *addin)
{
  return g_strdup (_("From an existing project on this computer"));
}

static void
ide_directory_genesis_addin_selection_changed (IdeDirectoryGenesisAddin *self,
                                               GtkFileChooserWidget     *chooser)
{
  g_assert (IDE_IS_DIRECTORY_GENESIS_ADDIN (self));
  g_assert (GTK_IS_FILE_CHOOSER_WIDGET (chooser));

  g_object_notify (G_OBJECT (self), "is-ready");
}

static void
ide_directory_genesis_addin_add_filters (GtkFileChooser *chooser)
{
  PeasEngine *engine = peas_engine_get_default ();
  const GList *list = peas_engine_get_plugin_list (engine);

  for (; list != NULL; list = list->next)
    {
      PeasPluginInfo *plugin_info = list->data;
      GtkFileFilter *filter;
      const gchar *pattern;
      const gchar *content_type;
      const gchar *name;
      gchar **patterns;
      gchar **content_types;
      gint i;

      if (!peas_plugin_info_is_loaded (plugin_info))
        continue;

      name = peas_plugin_info_get_external_data (plugin_info, "X-Project-File-Filter-Name");
      if (name == NULL)
        continue;

      pattern = peas_plugin_info_get_external_data (plugin_info, "X-Project-File-Filter-Pattern");
      content_type = peas_plugin_info_get_external_data (plugin_info, "X-Project-File-Filter-Content-Type");

      if (pattern == NULL && content_type == NULL)
        continue;

      patterns = g_strsplit (pattern ?: "", ",", 0);
      content_types = g_strsplit (content_type ?: "", ",", 0);

      filter = gtk_file_filter_new ();

      gtk_file_filter_set_name (filter, name);

      for (i = 0; patterns [i] != NULL; i++)
        {
          if (*patterns [i])
            gtk_file_filter_add_pattern (filter, patterns [i]);
        }

      for (i = 0; content_types [i] != NULL; i++)
        {
          if (*content_types [i])
            gtk_file_filter_add_mime_type (filter, content_types [i]);
        }

      gtk_file_chooser_add_filter (chooser, filter);

      g_strfreev (patterns);
      g_strfreev (content_types);
    }
}

static GtkWidget *
ide_directory_genesis_addin_get_widget (IdeGenesisAddin *addin)
{
  IdeDirectoryGenesisAddin *self = (IdeDirectoryGenesisAddin *)addin;

  g_assert (IDE_IS_DIRECTORY_GENESIS_ADDIN (self));

  if (self->widget == NULL)
    {
      self->widget = g_object_new (GTK_TYPE_FILE_CHOOSER_WIDGET,
                                   "action", GTK_FILE_CHOOSER_ACTION_OPEN,
                                   "visible", TRUE,
                                   NULL);
      g_signal_connect_object (self->widget,
                               "selection-changed",
                               G_CALLBACK (ide_directory_genesis_addin_selection_changed),
                               self,
                               G_CONNECT_SWAPPED);
      ide_directory_genesis_addin_add_filters (GTK_FILE_CHOOSER (self->widget));
    }

  return GTK_WIDGET (self->widget);
}

static void
ide_directory_genesis_addin_run_cb (GObject      *object,
                                    GAsyncResult *result,
                                    gpointer      user_data)
{
  IdeWorkbench *workbench = (IdeWorkbench *)object;
  g_autoptr(GTask) task = user_data;
  GError *error = NULL;

  g_assert (IDE_IS_WORKBENCH (workbench));
  g_assert (G_IS_TASK (task));

  if (!ide_workbench_open_project_finish (workbench, result, &error))
    g_task_return_error (task, error);
  else
    g_task_return_boolean (task, TRUE);
}

static void
ide_directory_genesis_addin_run_async (IdeGenesisAddin     *addin,
                                       GCancellable        *cancellable,
                                       GAsyncReadyCallback  callback,
                                       gpointer             user_data)
{
  IdeDirectoryGenesisAddin *self = (IdeDirectoryGenesisAddin *)addin;
  g_autoptr(GTask) task = NULL;
  g_autoptr(GFile) project_file = NULL;
  IdeWorkbench *workbench;

  g_assert (IDE_IS_DIRECTORY_GENESIS_ADDIN (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  workbench = ide_widget_get_workbench (GTK_WIDGET (self->widget));
  project_file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (self->widget));

  ide_workbench_open_project_async (workbench,
                                    project_file,
                                    cancellable,
                                    ide_directory_genesis_addin_run_cb,
                                    g_object_ref (task));
}

static gboolean
ide_directory_genesis_addin_run_finish (IdeGenesisAddin  *addin,
                                        GAsyncResult     *result,
                                        GError          **error)
{
  g_return_val_if_fail (IDE_IS_DIRECTORY_GENESIS_ADDIN (addin), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
genesis_addin_iface_init (IdeGenesisAddinInterface *iface)
{
  iface->get_title = ide_directory_genesis_addin_get_title;
  iface->get_icon_name = ide_directory_genesis_addin_get_icon_name;
  iface->get_widget = ide_directory_genesis_addin_get_widget;
  iface->run_async = ide_directory_genesis_addin_run_async;
  iface->run_finish = ide_directory_genesis_addin_run_finish;
}
