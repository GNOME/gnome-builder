/* ide-template-base.c
 *
 * Copyright 2016-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-template-base"

#include "config.h"

#include <glib/gstdio.h>
#include <errno.h>
#include <libide-threading.h>
#include <string.h>

#include "ide-template-base.h"
#include "ide-template-locator.h"

#define TIMEOUT_INTERVAL_MSEC 17
#define TIMEOUT_DURATION_MSEC  2

typedef struct
{
  TmplTemplateLocator *locator;
  GArray              *files;

  guint                has_expanded : 1;
} IdeTemplateBasePrivate;

typedef struct
{
  GFile        *file;
  GInputStream *stream;
  TmplScope    *scope;
  GFile        *destination;
  TmplTemplate *template;
  gchar        *result;
  gint          mode;
} FileExpansion;

typedef struct
{
  GArray    *files;
  guint      index;
  guint      completed;
} ExpansionTask;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (IdeTemplateBase, ide_template_base, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_LOCATOR,
  LAST_PROP
};

static GParamSpec *properties [LAST_PROP];

static void
ide_template_base_mkdirs_worker (IdeTask      *task,
                                 gpointer      source_object,
                                 gpointer      task_data,
                                 GCancellable *cancellable)
{
  IdeTemplateBase *self = source_object;
  IdeTemplateBasePrivate *priv = ide_template_base_get_instance_private (self);

  g_assert (IDE_IS_TASK (task));
  g_assert (IDE_IS_TEMPLATE_BASE (self));

  for (guint i = 0; i < priv->files->len; i++)
    {
      FileExpansion *fexp = &g_array_index (priv->files, FileExpansion, i);
      g_autoptr(GFile) directory = NULL;
      g_autoptr(GError) error = NULL;

      directory = g_file_get_parent (fexp->destination);

      if (!g_file_make_directory_with_parents (directory, cancellable, &error))
        {
          if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_EXISTS))
            {
              ide_task_return_error (task, g_steal_pointer (&error));
              return;
            }
        }
    }

  ide_task_return_boolean (task, TRUE);
}

static void
ide_template_base_mkdirs_async (IdeTemplateBase     *self,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;

  g_return_if_fail (IDE_IS_TEMPLATE_BASE (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_run_in_thread (task, ide_template_base_mkdirs_worker);
}

static gboolean
ide_template_base_mkdirs_finish (IdeTemplateBase  *self,
                                 GAsyncResult     *result,
                                 GError          **error)
{
  g_assert (IDE_IS_TEMPLATE_BASE (self));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

/**
 * ide_template_base_get_locator:
 * @self: An #IdeTemplateBase
 *
 * Fetches the #TmplTemplateLocator used for resolving templates.
 *
 * Returns: (transfer none) (nullable): a #TmplTemplateLocator or %NULL.
 */
TmplTemplateLocator *
ide_template_base_get_locator (IdeTemplateBase *self)
{
  IdeTemplateBasePrivate *priv = ide_template_base_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_TEMPLATE_BASE (self), NULL);

  return priv->locator;
}

void
ide_template_base_set_locator (IdeTemplateBase     *self,
                               TmplTemplateLocator *locator)
{
  IdeTemplateBasePrivate *priv = ide_template_base_get_instance_private (self);

  g_return_if_fail (IDE_IS_TEMPLATE_BASE (self));
  g_return_if_fail (!locator || TMPL_IS_TEMPLATE_LOCATOR (locator));

  if (priv->has_expanded)
    {
      g_warning ("Cannot change template locator after "
                 "ide_template_base_expand_all_async() has been called.");
      return;
    }

  if (g_set_object (&priv->locator, locator))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_LOCATOR]);
}

static void
clear_file_expansion (gpointer data)
{
  FileExpansion *expansion = data;

  g_clear_object (&expansion->file);
  g_clear_object (&expansion->stream);
  g_clear_pointer (&expansion->scope, tmpl_scope_unref);
  g_clear_object (&expansion->destination);
  g_clear_object (&expansion->template);
  g_clear_pointer (&expansion->result, g_free);
}

static void
ide_template_base_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  IdeTemplateBase *self = IDE_TEMPLATE_BASE(object);

  switch (prop_id)
    {
    case PROP_LOCATOR:
      g_value_set_object (value, ide_template_base_get_locator (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
ide_template_base_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  IdeTemplateBase *self = IDE_TEMPLATE_BASE(object);

  switch (prop_id)
    {
    case PROP_LOCATOR:
      ide_template_base_set_locator (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
ide_template_base_finalize (GObject *object)
{
  IdeTemplateBase *self = (IdeTemplateBase *)object;
  IdeTemplateBasePrivate *priv = ide_template_base_get_instance_private (self);

  g_clear_pointer (&priv->files, g_array_unref);
  g_clear_object (&priv->locator);

  G_OBJECT_CLASS (ide_template_base_parent_class)->finalize (object);
}

static void
ide_template_base_class_init (IdeTemplateBaseClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_template_base_finalize;
  object_class->get_property = ide_template_base_get_property;
  object_class->set_property = ide_template_base_set_property;

  /**
   * IdeTemplateBase:locator:
   *
   * The #IdeTemplateBase:locator property contains the #TmplTemplateLocator
   * that should be used to resolve template includes. If %NULL, templates
   * will not be allowed to include other templates.
   * directive.
   */
  properties [PROP_LOCATOR] =
    g_param_spec_object ("locator",
                         "Locator",
                         "Locator",
                         TMPL_TYPE_TEMPLATE_LOCATOR,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
ide_template_base_init (IdeTemplateBase *self)
{
  IdeTemplateBasePrivate *priv = ide_template_base_get_instance_private (self);

  priv->locator = TMPL_TEMPLATE_LOCATOR (ide_template_locator_new ());

  priv->files = g_array_new (FALSE, TRUE, sizeof (FileExpansion));
  g_array_set_clear_func (priv->files, clear_file_expansion);
}

static void
ide_template_base_parse_worker (IdeTask      *task,
                                gpointer      source_object,
                                gpointer      task_data,
                                GCancellable *cancellable)
{
  IdeTemplateBase *self = source_object;
  IdeTemplateBasePrivate *priv = ide_template_base_get_instance_private (self);

  g_assert (IDE_IS_TASK (task));
  g_assert (IDE_IS_TEMPLATE_BASE (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  for (guint i = 0; i < priv->files->len; i++)
    {
      FileExpansion *fexp = &g_array_index (priv->files, FileExpansion, i);
      g_autoptr(TmplTemplate) template = NULL;
      g_autoptr(GError) error = NULL;

      if (fexp->template != NULL)
        continue;

      template = tmpl_template_new (priv->locator);

      if (!tmpl_template_parse_file (template, fexp->file, cancellable, &error))
        {
          g_debug ("Failed to parse template: %s: %s",
                   g_file_peek_path (fexp->file),
                   error->message);
          ide_task_return_error (task, g_steal_pointer (&error));
          return;
        }

      fexp->template = g_object_ref (template);
    }

  ide_task_return_boolean (task, TRUE);
}

static void
ide_template_base_parse_async (IdeTemplateBase     *self,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;

  g_assert (IDE_IS_TEMPLATE_BASE (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_run_in_thread (task, ide_template_base_parse_worker);
}

static gboolean
ide_template_base_parse_finish (IdeTemplateBase  *self,
                                GAsyncResult     *result,
                                GError          **error)
{
  g_assert (IDE_IS_TEMPLATE_BASE (self));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static void
ide_template_base_replace_cb (GObject      *object,
                              GAsyncResult *result,
                              gpointer      user_data)
{
  GFile *file = (GFile *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  ExpansionTask *expansion;
  FileExpansion *fexp = NULL;
  guint i;

  g_assert (G_IS_FILE (file));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  expansion = ide_task_get_task_data (task);

  g_assert (expansion != NULL);
  g_assert (expansion->files != NULL);

  expansion->completed++;

  /*
   * Complete the file replacement operation.
   */
  if (!g_file_replace_contents_finish (file, result, NULL, &error))
    {
      if (!ide_task_get_completed (task))
        ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  /*
   * Locate the FileExpansion. We could remove this by tracking some
   * state in the callback, but that is more complex than it's worth
   * since we share the task between all the callbacks.
   */
  for (i = 0; i < expansion->files->len; i++)
    {
      FileExpansion *item = &g_array_index (expansion->files, FileExpansion, i);

      if (g_file_equal (item->destination, file))
        {
          fexp = item;
          break;
        }
    }

  /*
   * Unfortunately, we don't have a nice portable API to define modes.
   * So we limit our ability to chmod() to the local file-system.
   * This still works for things like FUSE, so much as they support
   * the posix chmod() API.
   */
  if ((fexp != NULL) && (fexp->mode != 0) && g_file_is_native (file))
    {
      g_autofree gchar *path = g_file_get_path (file);

      if (0 != g_chmod (path, fexp->mode))
        g_warning ("chmod(\"%s\", 0%o) failed with: %s",
                   path, fexp->mode, strerror (errno));
    }

  if (expansion->completed == expansion->files->len)
    {
      if (!ide_task_get_completed (task))
        ide_task_return_boolean (task, TRUE);
    }
}

static gboolean
ide_template_base_expand (IdeTask *task)
{
  ExpansionTask *expansion;
  gint64 end;
  gint64 now;

  g_assert (IDE_IS_TASK (task));

  expansion = ide_task_get_task_data (task);

  g_assert (expansion != NULL);
  g_assert (expansion->files != NULL);

  /*
   * We will only run for up to 2 milliseconds before we want to yield
   * back to the main loop and schedule future expansions as low-priority
   * so that we do not block the frame-clock;
   */
  for (end = (now = g_get_monotonic_time ()) + ((G_USEC_PER_SEC / 1000) * TIMEOUT_DURATION_MSEC);
       now < end;
       now = g_get_monotonic_time ())
    {
      FileExpansion *fexp;
      g_autoptr(GError) error = NULL;

      g_assert (expansion->index <= expansion->files->len);

      if (expansion->index == expansion->files->len)
        break;

      fexp = &g_array_index (expansion->files, FileExpansion, expansion->index);

      g_assert (fexp != NULL);
      g_assert (fexp->template != NULL);
      g_assert (fexp->scope != NULL);
      g_assert (fexp->result == NULL);

      fexp->result = tmpl_template_expand_string (fexp->template, fexp->scope, &error);

      if (fexp->result == NULL)
        {
          ide_task_return_error (task, g_steal_pointer (&error));
          return G_SOURCE_REMOVE;
        }

      expansion->index++;
    }

  /*
   * If we have completed expanding all the templates, we need to start
   * writing the results to the destination files asynchronously, and in
   * parallel. When all of the async operations have completed, we will
   * cleanup and complete the task.
   */
  if (expansion->index == expansion->files->len)
    {
      guint i;

      expansion->completed = 0;

      //ide_template_base_make_directories (task);

      for (i = 0; i < expansion->files->len; i++)
        {
          FileExpansion *fexp = &g_array_index (expansion->files, FileExpansion, i);

          g_assert (fexp != NULL);
          g_assert (G_IS_FILE (fexp->destination));
          g_assert (fexp->result != NULL);

          g_file_replace_contents_async (fexp->destination,
                                         fexp->result,
                                         strlen (fexp->result),
                                         NULL,
                                         FALSE,
                                         G_FILE_CREATE_REPLACE_DESTINATION,
                                         ide_task_get_cancellable (task),
                                         ide_template_base_replace_cb,
                                         g_object_ref (task));
        }

      return G_SOURCE_REMOVE;
    }

  return G_SOURCE_CONTINUE;
}

static void
ide_template_base_expand_parse_cb (GObject      *object,
                                   GAsyncResult *result,
                                   gpointer      user_data)
{
  IdeTemplateBase *self = (IdeTemplateBase *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_TEMPLATE_BASE (self));

  if (!ide_template_base_parse_finish (self, result, &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  g_timeout_add_full (G_PRIORITY_LOW,
                      TIMEOUT_INTERVAL_MSEC,
                      (GSourceFunc)ide_template_base_expand,
                      g_object_ref (task),
                      g_object_unref);
}

static void
ide_template_base_expand_mkdirs_cb (GObject      *object,
                                    GAsyncResult *result,
                                    gpointer      user_data)
{
  IdeTemplateBase *self = (IdeTemplateBase *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  GCancellable *cancellable;

  g_assert (IDE_IS_TEMPLATE_BASE (self));
  g_assert (IDE_IS_TASK (task));

  cancellable = ide_task_get_cancellable (task);

  if (!ide_template_base_mkdirs_finish (self, result, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_template_base_parse_async (self,
                                   cancellable,
                                   ide_template_base_expand_parse_cb,
                                   g_steal_pointer (&task));
}

void
ide_template_base_expand_all_async (IdeTemplateBase     *self,
                                    GCancellable        *cancellable,
                                    GAsyncReadyCallback  callback,
                                    gpointer             user_data)
{
  IdeTemplateBasePrivate *priv = ide_template_base_get_instance_private (self);
  g_autoptr(IdeTask) task = NULL;
  ExpansionTask *task_data;

  g_return_if_fail (IDE_IS_TEMPLATE_BASE (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task_data = g_new0 (ExpansionTask, 1);
  task_data->files = priv->files;
  task_data->index = 0;
  task_data->completed = 0;

  /*
   * The expand process will need to call tmpl_template_expand() and we want
   * that to happen in the main loop so that all scoped objects need not be
   * thread-safe.
   *
   * Therefore, the first step is to asynchronously load all of the templates
   * from storage. After that, we will expand the templates into memory,
   * being careful about how long we run per-cycle in the main-loop. If we
   * run too long, we risk adding jitter to the frame-clock and causing UI
   * elements to feel sluggish.
   *
   * Once we have all of our templates expanded, we progress to asynchronously
   * write them to the requested underlying storage.
   */
  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_task_data (task, task_data, g_free);

  /*
   * You can only call ide_template_base_expand_all_async() once, since we maintain
   * a bunch of state inline.
   */
  if (priv->has_expanded)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_PENDING,
                                 "%s() has already been called.",
                                 G_STRFUNC);
      return;
    }

  priv->has_expanded = TRUE;

  /*
   * If we have nothing to do, we still need to preserve our "executed" state.
   * So if there is nothing to do, short circuit now.
   */
  if (priv->files->len == 0)
    {
      ide_task_return_boolean (task, TRUE);
      return;
    }

  ide_template_base_mkdirs_async (self,
                                  cancellable,
                                  ide_template_base_expand_mkdirs_cb,
                                  g_object_ref (task));
}

gboolean
ide_template_base_expand_all_finish (IdeTemplateBase  *self,
                                     GAsyncResult     *result,
                                     GError          **error)
{
  g_return_val_if_fail (IDE_IS_TEMPLATE_BASE (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static TmplScope *
create_scope (IdeTemplateBase *self,
              TmplScope       *parent,
              GFile           *destination)
{
  TmplScope *scope;
  TmplSymbol *symbol;
  g_autofree gchar *filename = NULL;
  g_autofree gchar *year = NULL;
  g_autoptr(GDateTime) now = NULL;

  g_assert (IDE_IS_TEMPLATE_BASE (self));
  g_assert (G_IS_FILE (destination));

  scope = tmpl_scope_new_with_parent (parent);

  symbol = tmpl_scope_get (scope, "filename");
  filename = g_file_get_basename (destination);
  tmpl_symbol_assign_string (symbol, filename);

  now = g_date_time_new_now_local ();
  year = g_date_time_format (now, "%Y");
  symbol = tmpl_scope_get (scope, "year");
  tmpl_symbol_assign_string (symbol, year);

  return scope;
}

void
ide_template_base_add_resource (IdeTemplateBase *self,
                                const gchar     *resource_path,
                                GFile           *destination,
                                TmplScope       *scope,
                                gint             mode)
{
  IdeTemplateBasePrivate *priv = ide_template_base_get_instance_private (self);
  FileExpansion expansion = { 0 };
  g_autofree gchar *uri = NULL;

  g_return_if_fail (IDE_IS_TEMPLATE_BASE (self));
  g_return_if_fail (resource_path != NULL);
  g_return_if_fail (G_IS_FILE (destination));

  if (priv->has_expanded)
    {
      g_warning ("%s() called after ide_template_base_expand_all_async(). "
                 "Ignoring request to add resource.",
                 G_STRFUNC);
      return;
    }

  uri = g_strdup_printf ("resource://%s", resource_path);

  expansion.file = g_file_new_for_uri (uri);
  expansion.stream = NULL;
  expansion.scope = create_scope (self, scope, destination);
  expansion.destination = g_object_ref (destination);
  expansion.result = NULL;
  expansion.mode = mode;

  g_array_append_val (priv->files, expansion);
}

void
ide_template_base_add_path (IdeTemplateBase *self,
                            const gchar     *path,
                            GFile           *destination,
                            TmplScope       *scope,
                            gint             mode)
{
  IdeTemplateBasePrivate *priv = ide_template_base_get_instance_private (self);
  FileExpansion expansion = { 0 };

  g_return_if_fail (IDE_IS_TEMPLATE_BASE (self));
  g_return_if_fail (path != NULL);
  g_return_if_fail (G_IS_FILE (destination));

  if (priv->has_expanded)
    {
      g_warning ("%s() called after ide_template_base_expand_all_async(). "
                 "Ignoring request to add resource.",
                 G_STRFUNC);
      return;
    }

  expansion.file = g_file_new_for_path (path);
  expansion.stream = NULL;
  expansion.scope = create_scope (self, scope, destination);
  expansion.destination = g_object_ref (destination);
  expansion.result = NULL;
  expansion.mode = mode;

  g_array_append_val (priv->files, expansion);
}

void
ide_template_base_reset (IdeTemplateBase *self)
{
  IdeTemplateBasePrivate *priv = ide_template_base_get_instance_private (self);

  g_return_if_fail (IDE_IS_TEMPLATE_BASE (self));

  g_clear_pointer (&priv->files, g_array_unref);
  priv->files = g_array_new (FALSE, TRUE, sizeof (FileExpansion));

  priv->has_expanded = FALSE;
}
