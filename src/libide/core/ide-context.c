/* ide-context.c
 *
 * Copyright 2014-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-context"

#include "config.h"

#include <glib/gi18n.h>

#include <libpeas.h>

#include "ide-context.h"
#include "ide-context-private.h"
#include "ide-debug.h"
#include "ide-gsettings-action-group.h"
#include "ide-log-model-private.h"
#include "ide-macros.h"
#include "ide-marshal.h"
#include "ide-notifications.h"
#include "ide-settings.h"

/**
 * SECTION:ide-context
 * @title: IdeContext
 * @short_description: the root object for a project
 *
 * The #IdeContext object is the root object for a project. Everything
 * in a project is contained by this object.
 */

struct _IdeContext
{
  IdeObject       parent_instance;
  char           *project_id;
  char           *title;
  GFile          *workdir;
  IdeActionMuxer *action_muxer;
  IdeLogModel    *logs;
  guint           project_loaded : 1;
};

enum {
  PROP_0,
  PROP_PROJECT_ID,
  PROP_TITLE,
  PROP_WORKDIR,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (IdeContext, ide_context, IDE_TYPE_OBJECT)

static GParamSpec *properties [N_PROPS];
static const char *app_schema_ids[] = {
  "org.gnome.builder",
  "org.gnome.builder.code-insight",
  "org.gnome.builder.editor",
  "org.gnome.builder.project-tree",
  "org.gnome.builder.spelling",
  "org.gnome.builder.terminal",
};
static const char *project_schema_ids[] = {
  "org.gnome.builder.build",
  "org.gnome.builder.debug",
  "org.gnome.builder.project",
};

static gchar *
ide_context_repr (IdeObject *object)
{
  IdeContext *self = IDE_CONTEXT (object);

  return g_strdup_printf ("%s workdir=\"%s\" has_project=%d",
                          G_OBJECT_TYPE_NAME (self),
                          g_file_peek_path (self->workdir),
                          self->project_loaded);
}

static void
ide_context_engine_load_plugin_cb (IdeContext           *self,
                                   const PeasPluginInfo *plugin_info,
                                   PeasEngine           *engine)
{
  g_autofree char *schemas = NULL;

  g_assert (IDE_IS_CONTEXT (self));
  g_assert (plugin_info != NULL);
  g_assert (PEAS_IS_ENGINE (engine));

  if ((schemas = g_strdup (peas_plugin_info_get_external_data (plugin_info, "Settings-Schemas"))))
    {
      g_auto(GStrv) split = g_strsplit (g_strdelimit (schemas, " ,\t:", ';'), ";", 0);

      for (guint i = 0; split[i]; i++)
        {
          g_strstrip (split[i]);

          if (!ide_str_empty0 (split[i]))
            ide_context_register_settings (self, split[i]);
        }
    }
}

static void
ide_context_engine_unload_plugin_cb (IdeContext           *self,
                                     const PeasPluginInfo *plugin_info,
                                     PeasEngine           *engine)
{
  g_autofree char *schemas = NULL;

  g_assert (IDE_IS_CONTEXT (self));
  g_assert (plugin_info != NULL);
  g_assert (PEAS_IS_ENGINE (engine));

  if ((schemas = g_strdup (peas_plugin_info_get_external_data (plugin_info, "Settings-Schemas"))))
    {
      g_auto(GStrv) split = g_strsplit (g_strdelimit (schemas, " ,\t:", ';'), ";", 0);

      for (guint i = 0; split[i]; i++)
        {
          g_strstrip (split[i]);

          if (!ide_str_empty0 (split[i]))
            ide_context_unregister_settings (self, split[i]);
        }
    }
}

static void
ide_context_constructed (GObject *object)
{
  IdeContext *self = (IdeContext *)object;
  g_auto(GStrv) loaded_plugins = NULL;
  PeasEngine *engine;

  IDE_ENTRY;

  G_OBJECT_CLASS (ide_context_parent_class)->constructed (object);

  for (guint i = 0; i < G_N_ELEMENTS (app_schema_ids); i++)
    {
      g_autoptr(IdeSettings) settings = ide_settings_new (NULL, app_schema_ids[i]);
      g_autofree char *prefix = g_strconcat ("settings.app:", app_schema_ids[i], NULL);

      ide_action_muxer_insert_action_group (self->action_muxer,
                                            prefix,
                                            G_ACTION_GROUP (settings));
    }

  engine = peas_engine_get_default ();
  g_signal_connect_object (engine,
                           "load-plugin",
                           G_CALLBACK (ide_context_engine_load_plugin_cb),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (engine,
                           "unload-plugin",
                           G_CALLBACK (ide_context_engine_unload_plugin_cb),
                           self,
                           G_CONNECT_SWAPPED);

  loaded_plugins = peas_engine_dup_loaded_plugins (engine);

  for (guint i = 0; loaded_plugins[i]; i++)
    {
      const PeasPluginInfo *info = peas_engine_get_plugin_info (engine, loaded_plugins[i]);
      ide_context_engine_load_plugin_cb (self, info, engine);
    }

  IDE_EXIT;
}

static void
ide_context_destroy (IdeObject *object)
{
  IdeContext *self = (IdeContext *)object;

  g_assert (IDE_IS_OBJECT (object));

  if (self->action_muxer)
    {
      g_auto(GStrv) groups = ide_action_muxer_list_groups (self->action_muxer);

      for (guint i = 0; groups[i]; i++)
        ide_action_muxer_remove_action_group (self->action_muxer, groups[i]);
    }

  IDE_OBJECT_CLASS (ide_context_parent_class)->destroy (object);
}

static void
ide_context_finalize (GObject *object)
{
  IdeContext *self = (IdeContext *)object;

  g_clear_object (&self->action_muxer);
  g_clear_object (&self->workdir);
  g_clear_object (&self->logs);
  g_clear_pointer (&self->project_id, g_free);
  g_clear_pointer (&self->title, g_free);

  G_OBJECT_CLASS (ide_context_parent_class)->finalize (object);
}

static void
ide_context_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  IdeContext *self = IDE_CONTEXT (object);

  switch (prop_id)
    {
    case PROP_PROJECT_ID:
      g_value_take_string (value, ide_context_dup_project_id (self));
      break;

    case PROP_TITLE:
      g_value_take_string (value, ide_context_dup_title (self));
      break;

    case PROP_WORKDIR:
      g_value_take_object (value, ide_context_ref_workdir (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_context_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  IdeContext *self = IDE_CONTEXT (object);

  switch (prop_id)
    {
    case PROP_PROJECT_ID:
      ide_context_set_project_id (self, g_value_get_string (value));
      break;

    case PROP_TITLE:
      ide_context_set_title (self, g_value_get_string (value));
      break;

    case PROP_WORKDIR:
      ide_context_set_workdir (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_context_class_init (IdeContextClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeObjectClass *i_object_class = IDE_OBJECT_CLASS (klass);

  object_class->constructed = ide_context_constructed;
  object_class->finalize = ide_context_finalize;
  object_class->get_property = ide_context_get_property;
  object_class->set_property = ide_context_set_property;

  i_object_class->destroy = ide_context_destroy;
  i_object_class->repr = ide_context_repr;

  /**
   * IdeContext:project-id:
   *
   * The "project-id" property is the identifier to use when creating
   * files and folders for this project. It has a mutated form of either
   * the directory or some other discoverable trait of the project.
   *
   * It has also been modified to remove spaces and other unsafe
   * characters for file-systems.
   *
   * This may change during runtime, but usually only once when the
   * project has been initialize loaded.
   *
   * Before any project has loaded, this is "empty" to allow flexibility
   * for non-project use.
   */
  properties [PROP_PROJECT_ID] =
    g_param_spec_string ("project-id",
                         "Project Id",
                         "The project identifier used when creating files and folders",
                         "empty",
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  /**
   * IdeContext:title:
   *
   * The "title" property is a descriptive name for the project.
   */
  properties [PROP_TITLE] =
    g_param_spec_string ("title",
                         "Title",
                         "The title of the project",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  /**
   * IdeContext:workdir:
   *
   * The "workdir" property is the best guess at the working directory for the
   * context. This may be discovered using a common parent if multiple files
   * are opened without a project.
   */
  properties [PROP_WORKDIR] =
    g_param_spec_object ("workdir",
                         "Working Directory",
                         "The working directory for the project",
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_context_init (IdeContext *self)
{
  g_autoptr(IdeNotifications) notifs = NULL;

  self->workdir = g_file_new_for_path (g_get_home_dir ());
  self->project_id = g_strdup ("empty");
  self->title = g_strdup (_("Untitled"));
  self->action_muxer = ide_action_muxer_new ();

  notifs = ide_notifications_new ();
  ide_object_append (IDE_OBJECT (self), IDE_OBJECT (notifs));

  self->logs = _ide_log_model_new ();
}

/**
 * ide_context_new:
 *
 * Creates a new #IdeContext.
 *
 * This only creates the context object. After creating the object you need
 * to set a number of properties and then initialize asynchronously using
 * g_async_initable_init_async().
 *
 * Returns: (transfer full): an #IdeContext
 */
IdeContext *
ide_context_new (void)
{
  return ide_object_new (IDE_TYPE_CONTEXT, NULL);
}

static void
ide_context_peek_child_typed_cb (IdeObject *object,
                                 gpointer   user_data)
{
  struct {
    IdeObject *ret;
    GType      type;
  } *lookup = user_data;

  g_assert (IDE_IS_MAIN_THREAD ());

  if (lookup->ret != NULL)
    return;

  /* Take a borrowed instance, we're in the main thread so
   * we can ensure it's not fully destroyed.
   */
  if (G_TYPE_CHECK_INSTANCE_TYPE (object, lookup->type))
    lookup->ret = object;
}

/**
 * ide_context_peek_child_typed:
 * @self: a #IdeContext
 * @type: the #GType of the child
 *
 * Looks for the first child matching @type, and returns it. No reference is
 * taken to the child, so you should avoid using this except as used by
 * compatability functions.
 *
 * This may only be called from the main thread or you risk the objects
 * being finalized before your caller has a chance to reference them.
 *
 * Returns: (transfer none) (type IdeObject) (nullable): an #IdeObject that
 *   matches @type if successful; otherwise %NULL
 */
gpointer
ide_context_peek_child_typed (IdeContext *self,
                              GType       type)
{
  struct {
    IdeObject *ret;
    GType      type;
  } lookup = { NULL, type };

  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (IDE_IS_CONTEXT (self), NULL);

  ide_object_lock (IDE_OBJECT (self));
  ide_object_foreach (IDE_OBJECT (self), (GFunc)ide_context_peek_child_typed_cb, &lookup);
  ide_object_unlock (IDE_OBJECT (self));

  return lookup.ret;
}

/**
 * ide_context_dup_project_id:
 * @self: a #IdeContext
 *
 * Copies the project-id and returns it to the caller.
 *
 * Returns: (transfer full): a project-id as a string
 */
gchar *
ide_context_dup_project_id (IdeContext *self)
{
  gchar *ret;

  g_return_val_if_fail (IDE_IS_CONTEXT (self), NULL);

  ide_object_lock (IDE_OBJECT (self));
  ret = g_strdup (self->project_id);
  ide_object_unlock (IDE_OBJECT (self));

  g_return_val_if_fail (ret != NULL, NULL);

  return g_steal_pointer (&ret);
}

/**
 * ide_context_set_project_id:
 * @self: a #IdeContext
 *
 * Sets the project-id for the context.
 *
 * Generally, this should only be done once after loading a project.
 */
void
ide_context_set_project_id (IdeContext *self,
                            const char *project_id)
{
  gboolean register_settings = FALSE;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_CONTEXT (self));

  if (ide_str_empty0 (project_id))
    project_id = "empty";

  ide_object_lock (IDE_OBJECT (self));
  if (g_set_str (&self->project_id, project_id))
    {
      ide_object_notify_by_pspec (IDE_OBJECT (self), properties [PROP_PROJECT_ID]);
      register_settings = TRUE;
    }
  ide_object_unlock (IDE_OBJECT (self));

  if (register_settings)
    {
      for (guint i = 0; i < G_N_ELEMENTS (project_schema_ids); i++)
        ide_context_register_settings (self, project_schema_ids[i]);
    }
}

/**
 * ide_context_ref_workdir:
 * @self: a #IdeContext
 *
 * Gets the working-directory of the context and increments the
 * reference count by one.
 *
 * Returns: (transfer full): a #GFile
 */
GFile *
ide_context_ref_workdir (IdeContext *self)
{
  GFile *ret;

  g_return_val_if_fail (IDE_IS_CONTEXT (self), NULL);

  ide_object_lock (IDE_OBJECT (self));
  ret = g_object_ref (self->workdir);
  ide_object_unlock (IDE_OBJECT (self));

  return g_steal_pointer (&ret);
}

/**
 * ide_context_set_workdir:
 * @self: a #IdeContext
 * @workdir: a #GFile
 *
 * Sets the working directory for the project.
 *
 * This should generally only be set once after checking out the project.
 *
 * In future releases, changes may be made to change this in support of
 * git-worktrees or similar workflows.
 */
void
ide_context_set_workdir (IdeContext *self,
                         GFile      *workdir)
{
  g_return_if_fail (IDE_IS_CONTEXT (self));
  g_return_if_fail (G_IS_FILE (workdir));

  ide_object_lock (IDE_OBJECT (self));
  if (g_set_object (&self->workdir, workdir))
    ide_object_notify_by_pspec (G_OBJECT (self), properties [PROP_WORKDIR]);
  ide_object_unlock (IDE_OBJECT (self));
}

/**
 * ide_context_cache_file:
 * @self: a #IdeContext
 * @first_part: (nullable): The first part of the path
 *
 * Like ide_context_cache_filename() but returns a #GFile.
 *
 * Returns: (transfer full): a #GFile for the cache file
 */
GFile *
ide_context_cache_file (IdeContext  *self,
                        const gchar *first_part,
                        ...)
{
  g_autoptr(GPtrArray) ar = NULL;
  g_autofree gchar *path = NULL;
  g_autofree gchar *project_id = NULL;
  const gchar *part = first_part;
  va_list args;

  g_return_val_if_fail (IDE_IS_CONTEXT (self), NULL);

  project_id = ide_context_dup_project_id (self);

  ar = g_ptr_array_new ();
  g_ptr_array_add (ar, (gchar *)g_get_user_cache_dir ());
  g_ptr_array_add (ar, (gchar *)ide_get_program_name ());
  g_ptr_array_add (ar, (gchar *)"projects");
  g_ptr_array_add (ar, (gchar *)project_id);

  if (part != NULL)
    {
      va_start (args, first_part);
      do
        {
          g_ptr_array_add (ar, (gchar *)part);
          part = va_arg (args, const gchar *);
        }
      while (part != NULL);
      va_end (args);
    }

  g_ptr_array_add (ar, NULL);

  path = g_build_filenamev ((gchar **)ar->pdata);

  return g_file_new_for_path (path);
}

/**
 * ide_context_cache_filename:
 * @self: a #IdeContext
 * @first_part: the first part of the filename
 *
 * Creates a new filename that will be located in the projects cache directory.
 * This makes it convenient to remove files when a project is deleted as all
 * cache files will share a unified parent directory.
 *
 * The file will be located in a directory similar to
 * ~/.cache/gnome-builder/project_name. This may change based on the value
 * of g_get_user_cache_dir().
 *
 * Returns: (transfer full): A new string containing the cache filename
 */
gchar *
ide_context_cache_filename (IdeContext  *self,
                            const gchar *first_part,
                            ...)
{
  g_autofree gchar *project_id = NULL;
  g_autofree gchar *base = NULL;
  va_list args;
  gchar *ret;

  g_return_val_if_fail (IDE_IS_CONTEXT (self), NULL);

  project_id = ide_context_dup_project_id (self);

  g_return_val_if_fail (project_id != NULL, NULL);

  base = g_build_filename (g_get_user_cache_dir (),
                           ide_get_program_name (),
                           "projects",
                           project_id,
                           first_part,
                           NULL);

  if (first_part != NULL)
    {
      va_start (args, first_part);
      ret = g_build_filename_valist (base, &args);
      va_end (args);
    }
  else
    {
      ret = g_steal_pointer (&base);
    }

  return g_steal_pointer (&ret);
}

/**
 * ide_context_build_file:
 * @self: a #IdeContext
 * @path: (nullable): a path to the file
 *
 * Creates a new #GFile for the path.
 *
 * - If @path is %NULL, #IdeContext:workdir is returned.
 * - If @path is absolute, a new #GFile to the absolute path is returned.
 * - Otherwise, a #GFile child of #IdeContext:workdir is returned.
 *
 * Returns: (transfer full): a #GFile
 */
GFile *
ide_context_build_file (IdeContext  *self,
                        const gchar *path)
{
  g_autoptr(GFile) ret = NULL;

  g_return_val_if_fail (IDE_IS_CONTEXT (self), NULL);

  if (path == NULL)
    ret = g_file_dup (self->workdir);
  else if (g_path_is_absolute (path))
    ret = g_file_new_for_path (path);
  else
    ret = g_file_get_child (self->workdir, path);

  g_debug ("Creating file \"%s\" from \"%s\"", g_file_peek_path (ret), path);

  return g_steal_pointer (&ret);
}

/**
 * ide_context_build_filename:
 * @self: a #IdeContext
 * @first_part: first path part
 *
 * Creates a new path that starts from the working directory of the
 * loaded project.
 *
 * Returns: (transfer full): a string containing the new path
 */
gchar *
ide_context_build_filename (IdeContext  *self,
                            const gchar *first_part,
                            ...)
{
  g_autoptr(GPtrArray) ar = NULL;
  g_autoptr(GFile) workdir = NULL;
  const gchar *part = first_part;
  const gchar *base;
  va_list args;

  g_return_val_if_fail (IDE_IS_CONTEXT (self), NULL);
  g_return_val_if_fail (first_part != NULL, NULL);

  workdir = ide_context_ref_workdir (self);
  base = g_file_peek_path (workdir);

  ar = g_ptr_array_new ();

  /* If first part is absolute, just use that as our root */
  if (!g_path_is_absolute (first_part))
    g_ptr_array_add (ar, (gchar *)base);

  va_start (args, first_part);
  do
    {
      g_ptr_array_add (ar, (gchar *)part);
      part = va_arg (args, const gchar *);
    }
  while (part != NULL);
  va_end (args);

  g_ptr_array_add (ar, NULL);

  return g_build_filenamev ((gchar **)ar->pdata);
}

/**
 * ide_context_ref_project_settings:
 * @self: a #IdeContext
 *
 * Gets an org.gnome.builder.project #GSettings.
 *
 * This creates a new #GSettings instance for the project.
 *
 * Returns: (transfer full): a #GSettings
 */
GSettings *
ide_context_ref_project_settings (IdeContext *self)
{
  g_autofree gchar *path = NULL;

  g_return_val_if_fail (IDE_IS_CONTEXT (self), NULL);

  ide_object_lock (IDE_OBJECT (self));
  path = g_strdup_printf ("/org/gnome/builder/projects/%s/", self->project_id);
  ide_object_unlock (IDE_OBJECT (self));

  return g_settings_new_with_path ("org.gnome.builder.project", path);
}

/**
 * ide_context_ref_settings:
 * @self: a #IdeContext
 *
 * Gets an #IdeSettings for @schema_id.
 *
 * The #IdeSettings will read settings from project overrides before
 * falling back to application settings.
 *
 * Changes to settings will always apply to the project overrides.
 *
 * Returns: (transfer full) (nullable): an #IdeSettings
 */
IdeSettings *
ide_context_ref_settings (IdeContext *self,
                          const char *schema_id)
{
  g_autoptr(IdeActionMuxer) muxer = NULL;

  g_return_val_if_fail (IDE_IS_CONTEXT (self), NULL);
  g_return_val_if_fail (schema_id != NULL, NULL);

  if ((muxer = ide_context_ref_action_muxer (self)))
    {
      g_autofree char *prefix = g_strconcat ("settings.project:", schema_id, NULL);
      GActionGroup *group = ide_action_muxer_get_action_group (muxer, prefix);

      if (IDE_IS_SETTINGS (group))
        return g_object_ref (IDE_SETTINGS (group));
    }

  return NULL;
}

/**
 * ide_context_dup_title:
 * @self: a #IdeContext
 *
 * Returns: (transfer full): a string containing the title
 */
gchar *
ide_context_dup_title (IdeContext *self)
{
  gchar *ret;

  g_return_val_if_fail (IDE_IS_CONTEXT (self), NULL);

  ide_object_lock (IDE_OBJECT (self));
  ret = g_strdup (self->title);
  ide_object_unlock (IDE_OBJECT (self));

  return g_steal_pointer (&ret);
}

/**
 * ide_context_set_title:
 * @self: an #IdeContext
 * @title: (nullable): the title for the project or %NULL
 *
 * Sets the #IdeContext:title property. This is used by various
 * components to show the user the name of the project. This may
 * include the omnibar and the window title.
 */
void
ide_context_set_title (IdeContext  *self,
                       const gchar *title)
{
  g_return_if_fail (IDE_IS_CONTEXT (self));

  if (ide_str_empty0 (title))
    title = _("Untitled");

  ide_object_lock (IDE_OBJECT (self));
  if (g_set_str (&self->title, title))
    {
      ide_object_notify_by_pspec (IDE_OBJECT (self), properties [PROP_TITLE]);
    }
  ide_object_unlock (IDE_OBJECT (self));
}

void
ide_context_log (IdeContext     *self,
                 GLogLevelFlags  level,
                 const gchar    *domain,
                 const gchar    *message)
{
  g_assert (IDE_IS_CONTEXT (self));

  if (self->logs != NULL)
    _ide_log_model_append (self->logs, level, domain, message);

  g_log (domain, level, "%s", message);
}

/**
 * ide_context_has_project:
 * @self: a #IdeContext
 *
 * Checks to see if a project has been loaded in @context.
 *
 * Returns: %TRUE if a project has been, or is currently, loading.
 */
gboolean
ide_context_has_project (IdeContext *self)
{
  gboolean ret;

  g_return_val_if_fail (IDE_IS_CONTEXT (self), FALSE);

  ide_object_lock (IDE_OBJECT (self));
  ret = self->project_loaded;
  ide_object_unlock (IDE_OBJECT (self));

  return ret;
}

void
_ide_context_set_has_project (IdeContext *self)
{
  g_return_if_fail (IDE_IS_CONTEXT (self));

  ide_object_lock (IDE_OBJECT (self));
  self->project_loaded = TRUE;
  ide_object_unlock (IDE_OBJECT (self));
}

/**
 * ide_context_ref_action_muxer:
 * @self: a #IdeContext
 *
 * Gets the action muxer for the context.
 *
 * Returns: (transfer full): an #IdeActionMuxer
 */
IdeActionMuxer *
ide_context_ref_action_muxer (IdeContext *self)
{
  IdeActionMuxer *ret = NULL;

  g_return_val_if_fail (IDE_IS_CONTEXT (self), NULL);

  ide_object_lock (IDE_OBJECT (self));
  g_set_object (&ret, self->action_muxer);
  ide_object_unlock (IDE_OBJECT (self));

  return g_steal_pointer (&ret);
}

/**
 * ide_context_register_settings:
 * @self: a #IdeContext
 *
 * Registers settings that can be overriden by a project.
 *
 * Use this if you have a GSettings schema that can have both
 * application and per-project overrides.
 */
void
ide_context_register_settings (IdeContext *self,
                               const char *schema_id)
{
  g_autoptr(IdeActionMuxer) muxer = NULL;
  g_autofree char *project_id = NULL;

  g_return_if_fail (IDE_IS_CONTEXT (self));
  g_return_if_fail (schema_id != NULL);

  if ((muxer = ide_context_ref_action_muxer (self)) &&
      (project_id = ide_context_dup_project_id (self)))
    {
      g_autoptr(IdeSettings) project_settings = ide_settings_new (project_id, schema_id);
      g_autoptr(IdeSettings) app_settings = ide_settings_new (NULL, schema_id);
      g_autofree char *project_group = g_strconcat ("settings.project:", schema_id, NULL);
      g_autofree char *app_group = g_strconcat ("settings.app:", schema_id, NULL);

      ide_action_muxer_insert_action_group (muxer, app_group, G_ACTION_GROUP (app_settings));
      ide_action_muxer_insert_action_group (muxer, project_group, G_ACTION_GROUP (project_settings));
    }
}

void
ide_context_unregister_settings (IdeContext *self,
                                 const char *schema_id)
{
  g_autoptr(IdeActionMuxer) muxer = NULL;

  g_return_if_fail (IDE_IS_CONTEXT (self));
  g_return_if_fail (schema_id != NULL);

  if ((muxer = ide_context_ref_action_muxer (self)))
    {
      g_autofree char *project_group = g_strconcat ("settings.project:", schema_id, NULL);
      g_autofree char *app_group = g_strconcat ("settings.app:", schema_id, NULL);

      ide_action_muxer_insert_action_group (muxer, project_group, NULL);
      ide_action_muxer_insert_action_group (muxer, app_group, NULL);
    }
}

/**
 * ide_context_ref_logs:
 * @self: a #IdeContext
 *
 * Gets the logs for the context.
 *
 * Returns: (transfer full): a #GListModel of #IdeLogItem
 *
 * Since: 44
 */
GListModel *
ide_context_ref_logs (IdeContext *self)
{
  g_return_val_if_fail (IDE_IS_CONTEXT (self), NULL);

  return g_object_ref (G_LIST_MODEL (self->logs));
}
