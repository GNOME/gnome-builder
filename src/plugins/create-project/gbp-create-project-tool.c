/* gbp-create-project-tool.c
 *
 * Copyright © 2015 Christian Hergert <chergert@redhat.com>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <ctype.h>
#include <glib/gi18n.h>
#include <libpeas/peas.h>

#include "gbp-create-project-tool.h"

struct _GbpCreateProjectTool
{
  GObject    parent_instance;
  gboolean   list_templates;
  gchar    **args;
  gchar     *template;
  gchar     *language;
  gchar     *name;
  gchar     *vcs;
  GList     *project_templates;
};

static void application_tool_iface_init (IdeApplicationToolInterface *iface);

G_DEFINE_TYPE_EXTENDED (GbpCreateProjectTool, gbp_create_project_tool, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_APPLICATION_TOOL,
                                               application_tool_iface_init))

static void
template_providers_foreach_cb (PeasExtensionSet *set,
                               PeasPluginInfo   *plugin_info,
                               PeasExtension    *exten,
                               gpointer          user_data)
{
  GbpCreateProjectTool *self = user_data;
  IdeTemplateProvider *provider = IDE_TEMPLATE_PROVIDER (exten);
  GList *templates = ide_template_provider_get_project_templates (provider);

  self->project_templates = g_list_concat (self->project_templates, templates);
}

static void
gbp_create_project_tool_constructed (GObject *object)
{
  GbpCreateProjectTool *self = (GbpCreateProjectTool *)object;
  PeasEngine *engine = peas_engine_get_default ();
  PeasExtensionSet *extensions;

  extensions = peas_extension_set_new (engine,
                                       IDE_TYPE_TEMPLATE_PROVIDER,
                                       NULL);
  peas_extension_set_foreach (extensions,
                              template_providers_foreach_cb,
                              self);
  g_clear_object (&extensions);

  G_OBJECT_CLASS (gbp_create_project_tool_parent_class)->constructed (object);
}

static void
gbp_create_project_tool_finalize (GObject *object)
{
  GbpCreateProjectTool *self = (GbpCreateProjectTool *)object;

  g_list_foreach (self->project_templates, (GFunc)g_object_unref, NULL);
  g_clear_pointer (&self->project_templates, g_list_free);
  g_clear_pointer (&self->args, g_strfreev);
  g_clear_pointer (&self->language, g_free);
  g_clear_pointer (&self->template, g_free);
  g_clear_pointer (&self->name, g_free);
  g_clear_pointer (&self->vcs, g_free);

  G_OBJECT_CLASS (gbp_create_project_tool_parent_class)->finalize (object);
}

static void
gbp_create_project_tool_class_init (GbpCreateProjectToolClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = gbp_create_project_tool_constructed;
  object_class->finalize = gbp_create_project_tool_finalize;
}

static void
gbp_create_project_tool_init (GbpCreateProjectTool *self)
{
}

static void
gbp_create_project_tool_list_templates (GbpCreateProjectTool *self)
{
  const GList *iter;

  g_assert (GBP_IS_CREATE_PROJECT_TOOL (self));

  g_print ("\n");

  for (iter = self->project_templates; iter != NULL; iter = iter->next)
    {
      IdeProjectTemplate *template = iter->data;
      g_autofree gchar *id = NULL;

      if (NULL != (id = ide_project_template_get_id (template)))
        g_print ("  %s\n", id);
    }

  g_print ("\n");
}

static gboolean
gbp_create_project_tool_parse (GbpCreateProjectTool  *self,
                               GError               **error)
{
  g_autoptr(GOptionContext) context = NULL;
  GOptionEntry entries[] = {
    { "list-templates", 'l', 0, G_OPTION_ARG_NONE, &self->list_templates,
      N_("List available templates") },
    { "template", 't', 0, G_OPTION_ARG_STRING, &self->template,
      N_("Project template to generate") },
    { "language", 'g', 0, G_OPTION_ARG_STRING, &self->language,
      N_("The target language (if supported)") },
    { "vcs", 'v', 0, G_OPTION_ARG_STRING, &self->vcs,
      N_("The version control to use or “none” to disable"),
      N_("git") },
    { NULL }
  };

  g_assert (GBP_IS_CREATE_PROJECT_TOOL (self));

  context = g_option_context_new (_("create-project [OPTION...] PROJECT_NAME"));
  g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);

  if (!g_option_context_parse_strv (context, &self->args, error))
    return FALSE;

  return TRUE;
}

static IdeProjectTemplate *
find_template (GbpCreateProjectTool *self)
{
  const GList *iter;

  g_assert (GBP_IS_CREATE_PROJECT_TOOL (self));
  g_assert (self->template != NULL);

  for (iter = self->project_templates; iter != NULL; iter = iter->next)
    {
      IdeProjectTemplate *template = IDE_PROJECT_TEMPLATE (iter->data);
      g_autofree gchar *id = ide_project_template_get_id (template);

      if (g_strcmp0 (self->template, id) == 0)
        return template;
    }

  return NULL;
}

static gboolean
validate_name (GbpCreateProjectTool  *self,
               const gchar           *name,
               GError               **error)
{
  for (; *name; name = g_utf8_next_char (name))
    {
      gunichar ch = g_utf8_get_char (name);

      switch (ch)
        {
        default:
          if (isascii (ch))
            continue;
          /* Fall through */
        case '=':
        case ':':
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_INVALID_DATA,
                       _("Filename must be ASCII and may not contain : or ="));
          return FALSE;
        }
    }

  return TRUE;
}

static IdeVcsInitializer *
find_vcs (GbpCreateProjectTool *self)
{
  PeasPluginInfo *plugin_info;
  PeasEngine *engine;
  const gchar *name;

  g_assert (GBP_IS_CREATE_PROJECT_TOOL (self));

  name = self->vcs ?: "git";
  engine = peas_engine_get_default ();

  plugin_info = peas_engine_get_plugin_info (engine, name);

  /* allow use of "git" instead of "git-plugin" */
  if (plugin_info == NULL)
    {
      g_autofree gchar *fullname = g_strdup_printf ("%s-plugin", name);

      plugin_info = peas_engine_get_plugin_info (engine, fullname);
      if (plugin_info == NULL)
        return NULL;
    }


  return (IdeVcsInitializer *)peas_engine_create_extension (peas_engine_get_default (),
                                                            plugin_info,
                                                            IDE_TYPE_VCS_INITIALIZER,
                                                            NULL);

}

static void
vcs_init_cb (GObject      *object,
             GAsyncResult *result,
             gpointer      user_data)
{
  IdeVcsInitializer *vcs = (IdeVcsInitializer *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_VCS_INITIALIZER (vcs));
  g_assert (IDE_IS_TASK (task));
  g_assert (G_IS_ASYNC_RESULT (result));

  if (!ide_vcs_initializer_initialize_finish (vcs, result, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_int (task, 0);
}

static void
extract_cb (GObject      *object,
            GAsyncResult *result,
            gpointer      user_data)
{
  IdeProjectTemplate *template = (IdeProjectTemplate *)object;
  g_autoptr(IdeTask) task = user_data;
  GbpCreateProjectTool *self;
  g_autoptr(IdeVcsInitializer) vcs = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_PROJECT_TEMPLATE (template));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  self = ide_task_get_source_object (task);
  g_assert (GBP_IS_CREATE_PROJECT_TOOL (self));

  if (!ide_project_template_expand_finish (template, result, &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  vcs = find_vcs (self);

  if (vcs != NULL)
    {
      g_autoptr(GFile) project_dir = g_file_new_for_commandline_arg (self->name);

      ide_vcs_initializer_initialize_async (vcs,
                                            project_dir,
                                            ide_task_get_cancellable (task),
                                            vcs_init_cb,
                                            g_object_ref (task));
      return;
    }

  ide_task_return_int (task, 0);
}

static gboolean
extract_params (GbpCreateProjectTool  *self,
                GHashTable            *params,
                GError               **error)
{
  gint i;

  g_assert (GBP_IS_CREATE_PROJECT_TOOL (self));
  g_assert (params != NULL);

  if (self->args && g_strv_length (self->args) > 2)
    {
      for (i = 2; self->args [i]; i++)
        {
          const gchar *arg = self->args [i];
          const gchar *eq;

          if ((eq = strchr (arg, '=')) != NULL)
            {
              g_autofree gchar *value = NULL;
              gchar *key;
              GVariant *var;

              key = g_strndup (arg, (eq - arg));
              value = g_strdup (eq + 1);

              var = g_variant_parse (NULL, value, NULL, NULL, NULL);
              if (var == NULL)
                var = g_variant_new_string (value);

              g_hash_table_insert (params, key, g_variant_ref_sink (var));
            }
        }
    }

  return TRUE;
}

static void
gbp_create_project_tool_run_async (IdeApplicationTool  *tool,
                                   const gchar * const *arguments,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data)
{
  GbpCreateProjectTool *self = (GbpCreateProjectTool *)tool;
  IdeProjectTemplate *template;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GHashTable) params = NULL;
  const gchar *name;
  g_autoptr(GError) error = NULL;

  g_assert (GBP_IS_CREATE_PROJECT_TOOL (self));

  task = ide_task_new (self, cancellable, callback, user_data);

  /* pretend that "create-project" is argv[0] */
  self->args = g_strdupv ((gchar **)&arguments[1]);

  if (!gbp_create_project_tool_parse (self, &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  if (self->list_templates)
    {
      gbp_create_project_tool_list_templates (self);
      ide_task_return_int (task, 0);
      return;
    }

  if (!self->args || g_strv_length (self->args) < 2)
    {
      g_printerr (_("Please specify a project name.\n"));
      ide_task_return_int (task, 1);
      return;
    }

  name = self->args [1];

  if (!validate_name (self, name, &error))
    {
      g_printerr ("%s\n", error->message);
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  if (!self->template || !(template = find_template (self)))
    {
      g_printerr (_("Please specify a project template with --template=\n"));
      gbp_create_project_tool_list_templates (self);
      ide_task_return_int (task, 1);
      return;
    }

  params = g_hash_table_new_full (g_str_hash,
                                  g_str_equal,
                                  g_free,
                                  (GDestroyNotify)g_variant_unref);

  if (!extract_params (self, params, &error))
    {
      g_printerr ("%s\n", error->message);
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  g_hash_table_insert (params,
                       g_strdup ("name"),
                       g_variant_ref_sink (g_variant_new_string (name)));

  self->name = g_strdup (name);

  if (self->language != NULL)
    g_hash_table_insert (params,
                         g_strdup ("language"),
                         g_variant_ref_sink (g_variant_new_string (self->language)));

  g_hash_table_insert (params,
                       g_strdup ("versioning"),
                       g_variant_ref_sink (g_variant_new_string (self->vcs ?: "git")));

  ide_project_template_expand_async (template,
                                     params,
                                     NULL,
                                     extract_cb,
                                     g_object_ref (task));
}

static gint
gbp_create_project_tool_run_finish (IdeApplicationTool  *tool,
                                    GAsyncResult        *result,
                                    GError             **error)
{
  g_assert (GBP_IS_CREATE_PROJECT_TOOL (tool));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_int (IDE_TASK (result), error);
}

static void
application_tool_iface_init (IdeApplicationToolInterface *iface)
{
  iface->run_async = gbp_create_project_tool_run_async;
  iface->run_finish = gbp_create_project_tool_run_finish;
}
