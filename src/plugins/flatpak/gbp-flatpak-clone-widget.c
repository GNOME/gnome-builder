/* gbp-flatpak-clone-widget.c
 *
 * Copyright 2016 Endless Mobile, Inc.
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

#define G_LOG_DOMAIN "gbp-flatpak-clone-widget"

#include <dazzle.h>
#include <glib/gi18n.h>
#include <json-glib/json-glib.h>
#include <libide-greeter.h>
#include <libide-gui.h>
#include <libide-threading.h>
#include <libide-vcs.h>

#include "gbp-flatpak-clone-widget.h"
#include "gbp-flatpak-sources.h"

#define ANIMATION_DURATION_MSEC 250

struct _GbpFlatpakCloneWidget
{
  IdeSurface      parent_instance;

  /* Unowned */
  IdeContext     *context;
  GtkProgressBar *clone_progress;

  IdeNotification *notif;

  guint           is_ready : 1;

  gchar          *app_id_override;
  gchar          *child_name;
  gchar          *id;
  gchar          *manifest;

  guint           strip_components;
};

typedef enum {
  TYPE_GIT,
  TYPE_ARCHIVE
} SourceType;

typedef struct
{
  SourceType   type;
  IdeVcsUri   *uri;
  gchar       *branch;
  gchar       *sha;
  gchar       *name;
  gchar      **patches;
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

G_DEFINE_TYPE (GbpFlatpakCloneWidget, gbp_flatpak_clone_widget, IDE_TYPE_SURFACE)

static void
module_source_free (void *data)
{
  ModuleSource *src = data;

  g_clear_pointer (&src->uri, ide_vcs_uri_unref);
  g_free (src->branch);
  g_free (src->sha);
  g_strfreev (src->patches);
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
gbp_flatpak_clone_widget_set_manifest (GbpFlatpakCloneWidget *self,
                                       const gchar           *manifest)
{
  g_autofree gchar *name = NULL;
  g_autofree gchar *title = NULL;
  const gchar *ptr;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_FLATPAK_CLONE_WIDGET (self));
  g_assert (manifest != NULL);

  g_clear_pointer (&self->manifest, g_free);
  g_clear_pointer (&self->app_id_override, g_free);

  name = g_path_get_basename (manifest);
  /* translators: %s is replaced with the name of the flatpak manifest */
  title = g_strdup_printf (_("Cloning project %s"), name);
  ide_surface_set_title (IDE_SURFACE (self), title);

  /* if the filename does not end with .json, just set it right away,
   * even if it may fail later.
   */
  if (!(ptr = g_strrstr (manifest, ".json")))
    {
      self->manifest = g_strdup (manifest);
      return;
    }

  /* search for the first '+' after the .json extension */
  if (!(ptr = strchr (ptr, '+')))
    {
      self->manifest = g_strdup (manifest);
      return;
    }

  self->manifest = g_strndup (manifest, strlen (manifest) - strlen (ptr));
  self->app_id_override = g_strdup (ptr + 1);
}

static void
gbp_flatpak_clone_widget_finalize (GObject *object)
{
  GbpFlatpakCloneWidget *self = (GbpFlatpakCloneWidget *)object;

  g_clear_pointer (&self->app_id_override, g_free);
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
      gbp_flatpak_clone_widget_set_manifest (self, g_value_get_string (value));
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
  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/flatpak/gbp-flatpak-clone-widget.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpFlatpakCloneWidget, clone_progress);
}

static void
gbp_flatpak_clone_widget_init (GbpFlatpakCloneWidget *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
  self->notif = ide_notification_new ();
  self->strip_components = 1;
}

static gboolean
open_after_timeout (gpointer user_data)
{
  g_autoptr(IdeProjectInfo) project_info = NULL;
  g_autoptr(IdeTask) task = user_data;
  GbpFlatpakCloneWidget *self;
  DownloadRequest *req;
  GtkWidget *workspace;

  IDE_ENTRY;

  req = ide_task_get_task_data (task);
  self = ide_task_get_source_object (task);

  g_assert (GBP_IS_FLATPAK_CLONE_WIDGET (self));
  g_assert (req != NULL);
  g_assert (G_IS_FILE (req->project_file));

  /* Maybe we were shut mid-operation? */
  if (!(workspace = gtk_widget_get_ancestor (GTK_WIDGET (self), IDE_TYPE_GREETER_WORKSPACE)))
    IDE_RETURN (G_SOURCE_REMOVE);

  project_info = ide_project_info_new ();
  ide_project_info_set_file (project_info, req->project_file);
  ide_project_info_set_directory (project_info, req->project_file);

  ide_greeter_workspace_open_project (IDE_GREETER_WORKSPACE (workspace), project_info);

  IDE_RETURN (G_SOURCE_REMOVE);
}

static void
gbp_flatpak_clone_widget_worker_completed (IdeTask    *task,
                                           GParamSpec *pspec,
                                           gpointer    user_data)
{
  GbpFlatpakCloneWidget *self = user_data;

  if (!ide_task_get_completed (task))
    return;

  dzl_object_animate_full (self->clone_progress,
                           DZL_ANIMATION_EASE_IN_OUT_QUAD,
                           ANIMATION_DURATION_MSEC,
                           NULL,
                           (GDestroyNotify)dzl_gtk_widget_hide_with_fade,
                           self->clone_progress,
                           "fraction", 1.0,
                           NULL);

  if (ide_task_had_error (task))
    return;

  /* Wait for a second so animations can complete before opening
   * the project. Otherwise, it's pretty jarring to the user.
   */
  g_timeout_add (ANIMATION_DURATION_MSEC, open_after_timeout, g_object_ref (task));
}

static gboolean
check_directory_exists_and_nonempty (GFile         *directory,
                                     gboolean      *out_directory_exists_and_nonempty,
                                     GCancellable  *cancellable,
                                     GError       **error)
{
  g_autoptr(GError) local_error = NULL;
  g_autoptr(GFileEnumerator) enumerator = NULL;
  g_autoptr(GFileInfo) info = NULL;
  g_autofree gchar *path = g_file_get_path (directory);

  g_assert (G_IS_FILE (directory));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_assert (out_directory_exists_and_nonempty != NULL);

  *out_directory_exists_and_nonempty = FALSE;
  enumerator = g_file_enumerate_children (directory,
                                          G_FILE_ATTRIBUTE_STANDARD_NAME,
                                          G_FILE_QUERY_INFO_NONE,
                                          cancellable,
                                          &local_error);

  if (enumerator == NULL)
    {
      if (g_error_matches (local_error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
        {
          *out_directory_exists_and_nonempty = FALSE;
          return TRUE;
        }

      g_propagate_error (error, g_steal_pointer (&local_error));
      return FALSE;
    }

  /* Check for at least one child in the directory - if there is
   * one then the directory is non-empty. Otherwise, the directory
   * is either empty or an error occurred. */
  info = g_file_enumerator_next_file (enumerator, cancellable, &local_error);

  if (info != NULL)
    {
      *out_directory_exists_and_nonempty = TRUE;
      return TRUE;
    }

  if (local_error != NULL)
    {
      g_propagate_error (error, g_steal_pointer (&local_error));
      return FALSE;
    }

  *out_directory_exists_and_nonempty = FALSE;
  return TRUE;
}

static gboolean
download_flatpak_sources_if_required (GbpFlatpakCloneWidget  *self,
                                      DownloadRequest        *req,
                                      GCancellable           *cancellable,
                                      gboolean               *out_did_download,
                                      GError                **error)
{
  g_autofree gchar *uristr = NULL;
  g_autoptr(GError) local_error = NULL;

  g_assert (req != NULL);
  g_assert (out_did_download != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  *out_did_download = FALSE;

  if (req->src->type == TYPE_GIT)
    {
      g_autoptr(IdeNotification) notif = ide_notification_new ();

      uristr = ide_vcs_uri_to_string (req->src->uri);

      /* Only safe because notifications come from main-thread */
      g_object_bind_property (notif, "progress", self->clone_progress, "fraction", 0);

      if (!ide_vcs_cloner_clone_simple (self->context,
                                        "git",
                                        uristr,
                                        req->src->branch,
                                        g_file_peek_path (req->destination),
                                        notif,
                                        cancellable,
                                        error))
        return FALSE;

      *out_did_download = TRUE;

      req->project_file = g_file_dup (req->destination);
    }
  else if (req->src->type == TYPE_ARCHIVE)
    {
      g_autoptr(GFile) source_dir = g_file_get_child (req->destination,
                                                      req->src->name);
      gboolean exists_and_nonempty = FALSE;

      if (!check_directory_exists_and_nonempty (source_dir,
                                                &exists_and_nonempty,
                                                cancellable,
                                                &local_error))
        {
          g_propagate_error (error, g_steal_pointer (&local_error));
          return FALSE;
        }

      /*
       * If the target directory already exists and was non-empty,
       * then we have probably already checked out this project
       * using GbpFlatpakCloneWidget. In that case, we don't want
       * to overwrite it, we should get just return the source_dir
       * directly.
       */
      if (exists_and_nonempty)
        {
          g_debug ("Re-using non-empty source dir %s already at destination",
                   req->src->name);
          req->project_file = g_steal_pointer (&source_dir);
          *out_did_download = FALSE;
        }
      else
        {
          uristr = ide_vcs_uri_to_string (req->src->uri);
          g_debug ("Fetching source archive from %s", uristr);

          req->project_file = gbp_flatpak_sources_fetch_archive (uristr,
                                                                 req->src->sha,
                                                                 req->src->name,
                                                                 req->destination,
                                                                 self->strip_components,
                                                                 &local_error);
          if (local_error != NULL)
            {
              g_propagate_error (error, g_steal_pointer (&local_error));
              return FALSE;
            }

          *out_did_download = TRUE;
        }
    }

  return TRUE;
}

static void
gbp_flatpak_clone_widget_worker (IdeTask      *task,
                                 gpointer      source_object,
                                 gpointer      task_data,
                                 GCancellable *cancellable)
{
  GbpFlatpakCloneWidget *self = source_object;
  DownloadRequest *req = task_data;
  gboolean did_download = FALSE;
  g_autoptr(GFile) src = NULL;
  g_autoptr(GFile) dst = NULL;
  g_autoptr(GFile) build_config = NULL;
  g_autoptr(GKeyFile) build_config_keyfile = NULL;
  g_autofree gchar *manifest_contents = NULL;
  g_autofree gchar *build_config_path = NULL;
  g_autofree gchar *manifest_hash = NULL;
  g_autofree gchar *runtime_id = NULL;
  g_autofree gchar *manifest_file_name = NULL;
  g_autoptr(GError) error = NULL;
  gsize manifest_contents_len;

  g_assert (IDE_IS_TASK (task));
  g_assert (GBP_IS_FLATPAK_CLONE_WIDGET (self));
  g_assert (req != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  if (!download_flatpak_sources_if_required (self,
                                             req,
                                             cancellable,
                                             &did_download,
                                             &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  /* No need to do any of the following, we can assume that
   * we already have it there. */
  if (!did_download)
    {
      ide_task_return_boolean (task, TRUE);
      return;
    }

  for (guint i = 0; req->src->patches[i]; i++)
    {
      if (!gbp_flatpak_sources_apply_patch (req->src->patches[i],
                                            req->project_file,
                                            self->strip_components,
                                            &error))
      {
          ide_task_return_error (task, g_steal_pointer (&error));
          return;
        }
    }

  /* copy manifest into the source directory */
  src = g_file_new_for_path (self->manifest);
  manifest_file_name = g_strjoin (".", self->id, "json", NULL);
  dst = g_file_get_child (req->project_file, manifest_file_name);
  if (!g_file_copy (src, dst, G_FILE_COPY_OVERWRITE, NULL, NULL, NULL, &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  /* write a minimal build configuration file if it's not there yet */
  build_config = g_file_get_child (req->project_file, ".buildconfig");
  if (g_file_query_exists (build_config, NULL))
    {
      ide_task_return_boolean (task, TRUE);
      return;
    }

  if (!g_file_get_contents (self->manifest, &manifest_contents, &manifest_contents_len, &error))
    {
      /* don't make this error fatal, but log a warning */
      g_warning ("Failed to load JSON manifest at %s: %s",
                 self->manifest, error->message);
      g_clear_error (&error);
      ide_task_return_boolean (task, TRUE);
      return;
    }

  build_config_keyfile = g_key_file_new ();
  g_key_file_set_string (build_config_keyfile, "default", "default", "true");
  g_key_file_set_string (build_config_keyfile, "default", "device", "local");
  g_key_file_set_string (build_config_keyfile, "default", "name", "Default");

  manifest_hash = g_compute_checksum_for_data (G_CHECKSUM_SHA1,
                                               (const guchar *) manifest_contents,
                                               manifest_contents_len);
  runtime_id = g_strdup_printf ("%s.json@%s", self->id, manifest_hash);
  g_key_file_set_string (build_config_keyfile, "default", "runtime", runtime_id);
  g_debug ("Setting project runtime id %s", runtime_id);

  if (self->app_id_override != NULL)
    {
      g_key_file_set_string (build_config_keyfile, "default", "app-id", self->app_id_override);
      g_debug ("Setting project app ID override %s", self->app_id_override);
    }

  build_config_path = g_file_get_path (build_config);
  if (!g_key_file_save_to_file (build_config_keyfile, build_config_path, &error))
    {
      g_warning ("Failed to save %s: %s", build_config_path, error->message);
      g_clear_error (&error);
    }

  ide_task_return_boolean (task, TRUE);
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
  GPtrArray *patches;

  parser = json_parser_new ();
  if (!json_parser_load_from_file (parser, self->manifest, error))
    return NULL;

  patches = g_ptr_array_new ();
  root_node = json_parser_get_root (parser);
  root_object = json_node_get_object (root_node);

  if (json_object_has_member (root_object, "app-id"))
    self->id = g_strdup (json_object_get_string_member (root_object, "app-id"));
  else if (json_object_has_member (root_object, "id"))
    self->id = g_strdup (json_object_get_string_member (root_object, "id"));

  if (self->id == NULL)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_DATA,
                   "No app-id set in flatpak manifest %s",
                   self->manifest);
      return NULL;
    }

  modules = json_object_get_array_member (root_object, "modules");
  num_modules = json_array_get_length (modules);

  /* guess that the primary module is always the last one */
  app_object = json_array_get_object_element (modules, num_modules - 1);
  sources = json_object_get_array_member (app_object, "sources");

  src = g_slice_new0 (ModuleSource);
  src->name = g_strdup (json_object_get_string_member (app_object, "name"));

  for (guint i = 0; i < json_array_get_length (sources); i++)
    {
      JsonNode *source;
      JsonObject *source_object;
      const gchar *url;

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
          if (json_object_has_member (source_object, "path"))
            g_ptr_array_add (patches, g_strdup (json_object_get_string_member (source_object, "path")));
        }
    }

  g_ptr_array_add (patches, NULL);
  src->patches = (gchar **) g_ptr_array_free (patches, FALSE);

  return src;
}

void
gbp_flatpak_clone_widget_clone_async (GbpFlatpakCloneWidget   *self,
                                      GCancellable            *cancellable,
                                      GAsyncReadyCallback      callback,
                                      gpointer                 user_data)
{
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GFile) destination = NULL;
  g_autoptr(GError) error = NULL;
  DownloadRequest *req;
  ModuleSource *src;

  g_return_if_fail (GBP_IS_FLATPAK_CLONE_WIDGET (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_flatpak_clone_widget_clone_async);
  ide_task_set_release_on_propagate (task, FALSE);

  self->context = ide_widget_get_context (GTK_WIDGET (self));

  src = get_source (self, &error);
  if (src == NULL)
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  if (src->uri != NULL && src->type == TYPE_GIT)
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

  destination = g_file_new_for_path (ide_get_projects_dir ());

  if (self->child_name)
    {
      g_autoptr(GFile) child = g_file_get_child (destination, self->child_name);
      g_set_object (&destination, child);
    }

  req = download_request_new (src, destination);

  ide_task_set_task_data (task, req, download_request_free);
  ide_task_run_in_thread (task, gbp_flatpak_clone_widget_worker);

  g_signal_connect (task, "notify::completed",
                    G_CALLBACK (gbp_flatpak_clone_widget_worker_completed), self);
}

gboolean
gbp_flatpak_clone_widget_clone_finish (GbpFlatpakCloneWidget *self,
                                       GAsyncResult          *result,
                                       GError               **error)
{
  g_return_val_if_fail (GBP_IS_FLATPAK_CLONE_WIDGET (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}
