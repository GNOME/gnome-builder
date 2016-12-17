/* gbp-flatpak-clone-widget.c
 *
 * Copyright (C) 2016 Endless Mobile, Inc.
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
#include <libgit2-glib/ggit.h>
#include <ide.h>

#include "egg-animation.h"

#include "gbp-flatpak-clone-widget.h"
#include "gbp-flatpak-source-archive.h"

#define ANIMATION_DURATION_MSEC 250

struct _GbpFlatpakCloneWidget
{
  GtkBin          parent_instance;

  GtkProgressBar *clone_progress;

  guint           is_ready : 1;

  gchar          *child_name;
  gchar          *id;
  gchar          *manifest;
};

typedef enum {
  TYPE_GIT,
  TYPE_ARCHIVE,
  TYPE_PATCH
} SourceType;

typedef struct
{
  SourceType type;
  IdeVcsUri *uri;
  gchar     *branch;
  gchar     *sha;
  gchar     *path;
  gchar     *name;
} ModuleSource;

typedef struct
{
  ModuleSource *src;
  GFile        *destination;
  GFile        *project_file;
} DownloadRequest;

enum {
  PROP_0,
  PROP_IS_READY,
  PROP_MANIFEST,
  LAST_PROP
};

G_DEFINE_TYPE (GbpFlatpakCloneWidget, gbp_flatpak_clone_widget, GTK_TYPE_BIN)

static void
module_source_free (void *data)
{
  ModuleSource *src = data;

  g_clear_pointer (&src->uri, ide_vcs_uri_unref);
  g_free (src->branch);
  g_free (src->sha);
  g_free (src->path);
  g_free (src->name);
  g_slice_free (ModuleSource, src);
}

static void
download_request_free (gpointer data)
{
  DownloadRequest *req = data;

  module_source_free (req->src);
  g_clear_object (&req->destination);
  g_clear_object (&req->project_file);
  g_slice_free (DownloadRequest, req);
}

static DownloadRequest *
download_request_new (ModuleSource *src,
                      GFile        *destination)
{
  DownloadRequest *req;

  g_assert (src);
  g_assert (destination);

  req = g_slice_new0 (DownloadRequest);
  req->src = src;
  req->destination = g_object_ref (destination);

  return req;
}

static void
gbp_flatpak_clone_widget_finalize (GObject *object)
{
  GbpFlatpakCloneWidget *self = (GbpFlatpakCloneWidget *)object;

  g_clear_pointer (&self->child_name, g_free);
  g_clear_pointer (&self->id, g_free);
  g_clear_pointer (&self->manifest, g_free);

  G_OBJECT_CLASS (gbp_flatpak_clone_widget_parent_class)->finalize (object);
}

static void
gbp_flatpak_clone_widget_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  GbpFlatpakCloneWidget *self = GBP_FLATPAK_CLONE_WIDGET(object);
  switch (prop_id)
    {
    case PROP_MANIFEST:
      g_free (self->manifest);
      self->manifest = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
gbp_flatpak_clone_widget_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  GbpFlatpakCloneWidget *self = GBP_FLATPAK_CLONE_WIDGET(object);

  switch (prop_id)
    {
    case PROP_IS_READY:
      g_value_set_boolean (value, self->is_ready);
      break;

    case PROP_MANIFEST:
      g_value_set_string (value, self->manifest);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_flatpak_clone_widget_class_init (GbpFlatpakCloneWidgetClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gbp_flatpak_clone_widget_finalize;
  object_class->get_property = gbp_flatpak_clone_widget_get_property;
  object_class->set_property = gbp_flatpak_clone_widget_set_property;

  g_object_class_install_property (object_class,
                                   PROP_IS_READY,
                                   g_param_spec_boolean ("is-ready",
                                                         "Is Ready",
                                                         "If the widget is ready to continue.",
                                                         FALSE,
                                                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (object_class,
                                   PROP_MANIFEST,
                                   g_param_spec_string ("manifest",
                                                        "Manifest",
                                                        "Name of the flatpak manifest to load.",
                                                        NULL,
                                                        (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  gtk_widget_class_set_css_name (widget_class, "flatpakclonewidget");
  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/plugins/flatpak-plugin/gbp-flatpak-clone-widget.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpFlatpakCloneWidget, clone_progress);
}

static void
gbp_flatpak_clone_widget_init (GbpFlatpakCloneWidget *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

static void
ide_workbench_open_project_async_cb (GObject      *object,
                                     GAsyncResult *result,
                                     gpointer      user_data)
{
  GbpFlatpakCloneWidget *self = user_data;
  IdeWorkbench *workbench = IDE_WORKBENCH (object);
  IdeConfigurationManager *configmgr;
  IdeConfiguration *config;
  IdeContext *context;
  IdeContext *config_context;
  IdeRuntimeManager *runtime_manager;

  g_assert (IDE_IS_WORKBENCH (workbench));
  g_assert (GBP_IS_FLATPAK_CLONE_WIDGET (self));

  context = ide_workbench_get_context (IDE_WORKBENCH (workbench));
  configmgr = ide_context_get_configuration_manager (context);
  config = ide_configuration_manager_get_current (configmgr);

  config_context = ide_object_get_context (IDE_OBJECT (config));
  runtime_manager = ide_context_get_runtime_manager (config_context);

  if (g_list_model_get_n_items (G_LIST_MODEL (runtime_manager)) > 0)
    {
      g_autoptr(IdeRuntime) last =
        g_list_model_get_item (G_LIST_MODEL (runtime_manager),
                               g_list_model_get_n_items (G_LIST_MODEL (runtime_manager)) - 1);
      ide_configuration_set_runtime_id (config, ide_runtime_get_id (last));
    }
}

static gboolean
open_after_timeout (gpointer user_data)
{
  g_autoptr(GTask) task = user_data;
  DownloadRequest *req;
  GbpFlatpakCloneWidget *self;
  IdeWorkbench *workbench;

  IDE_ENTRY;

  req = g_task_get_task_data (task);
  self = g_task_get_source_object (task);
  g_assert (GBP_IS_FLATPAK_CLONE_WIDGET (self));

  workbench = ide_widget_get_workbench (GTK_WIDGET (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  ide_workbench_open_project_async (workbench, req->project_file, NULL,
                                    ide_workbench_open_project_async_cb, g_object_ref (self));

  IDE_RETURN (G_SOURCE_REMOVE);
}

static void
gbp_flatpak_clone_widget_worker_completed (GTask      *task,
                                           GParamSpec *pspec,
                                           gpointer    user_data)
{
  GbpFlatpakCloneWidget *self = user_data;

  if (!g_task_get_completed (task))
    return;

  egg_object_animate_full (self->clone_progress,
                           EGG_ANIMATION_EASE_IN_OUT_QUAD,
                           ANIMATION_DURATION_MSEC,
                           NULL,
                           (GDestroyNotify)ide_widget_hide_with_fade,
                           self->clone_progress,
                           "fraction", 1.0,
                           NULL);

  /* Wait for a second so animations can complete before opening
   * the project. Otherwise, it's pretty jarring to the user.
   */
  g_timeout_add (ANIMATION_DURATION_MSEC, open_after_timeout, g_object_ref (task));
}

static void
gbp_flatpak_clone_widget_worker (GTask        *task,
                                 gpointer      source_object,
                                 gpointer      task_data,
                                 GCancellable *cancellable)
{
  GbpFlatpakCloneWidget *self = source_object;
  DownloadRequest *req = task_data;
  g_autofree gchar *uristr = NULL;
  GgitFetchOptions *fetch_options;
  g_autoptr(GgitCloneOptions) clone_options = NULL;
  g_autoptr(GgitRemoteCallbacks) callbacks = NULL;
  g_autoptr(GgitRepository) repository = NULL;
  g_autoptr(IdeProgress) progress = NULL;
  g_autoptr(GFile) src = NULL;
  g_autoptr(GFile) dst = NULL;
  GError *error = NULL;
  GType git_callbacks_type;

  g_assert (G_IS_TASK (task));
  g_assert (GBP_IS_FLATPAK_CLONE_WIDGET (self));
  g_assert (req != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  if (req->src->type == TYPE_GIT)
    {
      /* First, try to open an existing repository at this path */
      repository = ggit_repository_open (req->destination, &error);

      if (repository == NULL &&
          !g_error_matches (error, GGIT_ERROR, GGIT_ERROR_NOTFOUND))
        {
          g_task_return_error (task, error);
          return;
        }

      g_clear_error (&error);

      if (repository == NULL)
        {
          /* HACK: we don't want libide to depend on libgit2 just yet, so for
           * now, we just lookup the GType of the object we need from the git
           * plugin by name.
           */
          git_callbacks_type = g_type_from_name ("IdeGitRemoteCallbacks");
          g_assert (git_callbacks_type != 0);

          callbacks = g_object_new (git_callbacks_type, NULL);
          g_object_get (callbacks, "progress", &progress, NULL);
          g_object_bind_property (progress, "fraction", self->clone_progress, "fraction", 0);

          fetch_options = ggit_fetch_options_new ();
          ggit_fetch_options_set_remote_callbacks (fetch_options, callbacks);

          clone_options = ggit_clone_options_new ();
          ggit_clone_options_set_is_bare (clone_options, FALSE);
          ggit_clone_options_set_checkout_branch (clone_options, req->src->branch);
          ggit_clone_options_set_fetch_options (clone_options, fetch_options);
          g_clear_pointer (&fetch_options, ggit_fetch_options_free);

          uristr = ide_vcs_uri_to_string (req->src->uri);
          repository = ggit_repository_clone (uristr, req->destination, clone_options, &error);
          if (repository == NULL)
            {
              g_task_return_error (task, error);
              return;
            }
        }
      req->project_file = ggit_repository_get_workdir (repository);
    }
  else if (req->src->type == TYPE_ARCHIVE)
    {
      uristr = ide_vcs_uri_to_string (req->src->uri);
      req->project_file = fetch_archive (uristr, req->src->sha, req->src->name, req->destination, &error);
    }

  /* copy manifest into the source directory */
  src = g_file_new_for_path (self->manifest);
  dst = g_file_get_child (req->project_file,
                          g_strjoin (".", self->id, "json", NULL));
  g_clear_pointer (&self->id, g_free);
  if (!g_file_copy (src, dst, G_FILE_COPY_OVERWRITE, NULL,
                    NULL, NULL, &error))
    {
      g_task_return_error (task, error);
      return;
    }

  g_task_return_boolean (task, TRUE);
}

static ModuleSource *
get_source (GbpFlatpakCloneWidget  *self,
            GError                **error)
{
  g_autoptr(JsonParser) parser = NULL;
  JsonNode *root_node;
  JsonObject *root_object;
  JsonObject *app_object;
  JsonArray *modules = NULL;
  JsonArray *sources = NULL;
  guint num_modules;
  ModuleSource *src;
  g_autoptr(IdeVcsUri) uri = NULL;

  parser = json_parser_new ();
  if (!json_parser_load_from_file (parser, self->manifest, error))
    return NULL;

  root_node = json_parser_get_root (parser);
  root_object = json_node_get_object (root_node);

  self->id = g_strdup (json_object_get_string_member (root_object, "app-id"));

  modules = json_object_get_array_member (root_object, "modules");
  num_modules = json_array_get_length (modules);

  /* guess that the primary module is always the last one */
  app_object = json_array_get_object_element (modules, num_modules - 1);
  sources = json_object_get_array_member (app_object, "sources");

  for (guint i = 0; i < json_array_get_length (sources); i++)
    {
      JsonNode *source;
      JsonObject *source_object;
      const gchar *url;

      src = g_slice_new0 (ModuleSource);
      src->name = g_strdup (json_object_get_string_member (app_object, "name"));

      source = json_array_get_element (sources, i);
      source_object = json_node_get_object (source);

      if (g_strcmp0 (json_object_get_string_member (source_object, "type"), "git") == 0)
        {
          src->type = TYPE_GIT;
          if (json_object_has_member (source_object, "branch"))
            src->branch = g_strdup (json_object_get_string_member (source_object, "branch"));

          url = json_object_get_string_member (source_object, "url");
          src->uri = ide_vcs_uri_new (url);
        }
      else if (strcmp (json_object_get_string_member(source_object, "type"), "archive") == 0)
        {
          src->type = TYPE_ARCHIVE;
          if (json_object_has_member (source_object, "sha256"))
            src->sha = g_strdup (json_object_get_string_member (source_object, "sha256"));

          url = json_object_get_string_member (source_object, "url");
          src->uri = ide_vcs_uri_new (url);
        }
      else if (g_strcmp0 (json_object_get_string_member(source_object, "type"), "patch") == 0)
        {
          src->type = TYPE_PATCH;
          if (json_object_has_member (source_object, "path"))
            src->path = g_strdup (json_object_get_string_member (source_object, "path"));
        }
    }

  return src;
}

void
gbp_flatpak_clone_widget_clone_async (GbpFlatpakCloneWidget   *self,
                                      GCancellable            *cancellable,
                                      GAsyncReadyCallback      callback,
                                      gpointer                 user_data)
{
  g_autoptr(GTask) task = NULL;
  g_autoptr(GSettings) settings = NULL;
  g_autoptr(GFile) destination = NULL;
  g_autofree gchar *path = NULL;
  g_autofree gchar *projects_dir = NULL;
  DownloadRequest *req;
  ModuleSource *src;
  GError *error = NULL;

  g_return_if_fail (GBP_IS_FLATPAK_CLONE_WIDGET (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);

  src = get_source (self, &error);
  if (src == NULL)
    {
      g_task_return_error (task, error);
      return;
    }

  if (src->uri != NULL)
    {
      const gchar *uri_path;
      gchar *name = NULL;

      uri_path = ide_vcs_uri_get_path (src->uri);
      if (uri_path != NULL)
        {
          name = g_path_get_basename (uri_path);

          if (g_str_has_suffix (name, ".git"))
            *(strrchr (name, '.')) = '\0';

          if (!g_str_equal (name, "/"))
            {
              g_free (self->child_name);
              self->child_name = g_steal_pointer (&name);
            }

          g_free (name);
        }
    }

  settings = g_settings_new ("org.gnome.builder");
  path = g_settings_get_string (settings, "projects-directory");

  if (ide_str_empty0 (path))
    path = g_build_filename (g_get_home_dir (), "Projects", NULL);

  if (!g_path_is_absolute (path))
    projects_dir = g_build_filename (g_get_home_dir (), path, NULL);
  else
    projects_dir = g_steal_pointer (&path);

  destination = g_file_new_for_path (projects_dir);

  if (self->child_name)
    {
      g_autoptr(GFile) child = g_file_get_child (destination, self->child_name);
      g_set_object (&destination, child);
    }

  req = download_request_new (src, destination);

  g_task_set_task_data (task, req, download_request_free);
  g_task_run_in_thread (task, gbp_flatpak_clone_widget_worker);

  g_signal_connect (task, "notify::completed",
                    G_CALLBACK (gbp_flatpak_clone_widget_worker_completed), self);
}

gboolean
gbp_flatpak_clone_widget_clone_finish (GbpFlatpakCloneWidget *self,
                                       GAsyncResult          *result,
                                       GError               **error)
{
  g_return_val_if_fail (GBP_IS_FLATPAK_CLONE_WIDGET (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}
