/* gbp-code-index-service.c
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-code-index-service"

#include "config.h"

#include <libide-code.h>
#include <libide-foundry.h>

#include "gbp-code-index-executor.h"
#include "gbp-code-index-plan.h"
#include "gbp-code-index-service.h"

#define DELAY_FOR_INDEXING_MSEC 500

struct _GbpCodeIndexService
{
  IdeObject         parent_instance;

  IdeNotification  *notif;
  GCancellable     *cancellable;

  guint             queued_source;

  guint             needs_indexing : 1;
  guint             indexing : 1;
  guint             started : 1;
  guint             paused : 1;
};

enum {
  PROP_0,
  PROP_PAUSED,
  N_PROPS
};

G_DEFINE_TYPE (GbpCodeIndexService, gbp_code_index_service, IDE_TYPE_OBJECT)

static void     gbp_code_index_service_index_async  (GbpCodeIndexService  *self,
                                                     GCancellable         *cancellable,
                                                     GAsyncReadyCallback   callback,
                                                     gpointer              user_data);
static gboolean gbp_code_index_service_index_finish (GbpCodeIndexService   *self,
                                                     GAsyncResult          *result,
                                                     GError              **error);

static GParamSpec *properties [N_PROPS];

static gchar *
gbp_code_index_service_repr (IdeObject *object)
{
  GbpCodeIndexService *self = (GbpCodeIndexService *)object;

  return g_strdup_printf ("%s started=%d paused=%d",
                          G_OBJECT_TYPE_NAME (self), self->started, self->paused);
}

static void
gbp_code_index_service_destroy (IdeObject *object)
{
  GbpCodeIndexService *self = (GbpCodeIndexService *)object;

  gbp_code_index_service_stop (self);

  IDE_OBJECT_CLASS (gbp_code_index_service_parent_class)->destroy (object);
}

static void
gbp_code_index_service_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  GbpCodeIndexService *self = GBP_CODE_INDEX_SERVICE (object);

  switch (prop_id)
    {
    case PROP_PAUSED:
      g_value_set_boolean (value, gbp_code_index_service_get_paused (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_code_index_service_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  GbpCodeIndexService *self = GBP_CODE_INDEX_SERVICE (object);

  switch (prop_id)
    {
    case PROP_PAUSED:
      gbp_code_index_service_set_paused (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_code_index_service_class_init (GbpCodeIndexServiceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeObjectClass *i_object_class = IDE_OBJECT_CLASS (klass);

  object_class->get_property = gbp_code_index_service_get_property;
  object_class->set_property = gbp_code_index_service_set_property;

  i_object_class->destroy = gbp_code_index_service_destroy;
  i_object_class->repr = gbp_code_index_service_repr;

  properties [PROP_PAUSED] =
    g_param_spec_boolean ("paused",
                          "Paused",
                          "If the service is paused",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_code_index_service_init (GbpCodeIndexService *self)
{
}

static void
gbp_code_index_service_index_cb (GObject      *object,
                                 GAsyncResult *result,
                                 gpointer      user_data)
{
  GbpCodeIndexService *self = (GbpCodeIndexService *)object;
  g_autoptr(GCancellable) cancellable = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODE_INDEX_SERVICE (self));
  g_assert (G_IS_ASYNC_RESULT (result));

  if (!gbp_code_index_service_index_finish (self, result, &error))
    g_warning ("Code indexing failed: %s", error->message);
}

static gboolean
gbp_code_index_service_queue_index_cb (gpointer user_data)
{
  GbpCodeIndexService *self = user_data;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODE_INDEX_SERVICE (self));

  self->queued_source = 0;

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);
  self->cancellable = g_cancellable_new ();

  gbp_code_index_service_index_async (self,
                                      self->cancellable,
                                      gbp_code_index_service_index_cb,
                                      NULL);

  IDE_RETURN (G_SOURCE_REMOVE);
}

static void
gbp_code_index_service_queue_index (GbpCodeIndexService *self)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODE_INDEX_SERVICE (self));

  self->needs_indexing = TRUE;

  if (self->indexing)
    return;

  g_clear_handle_id (&self->queued_source, g_source_remove);
  self->queued_source = g_timeout_add (DELAY_FOR_INDEXING_MSEC,
                                       gbp_code_index_service_queue_index_cb,
                                       self);
}

static void
gbp_code_index_service_pause (GbpCodeIndexService *self)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODE_INDEX_SERVICE (self));

  self->paused = TRUE;
  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PAUSED]);
}

static void
gbp_code_index_service_unpause (GbpCodeIndexService *self)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODE_INDEX_SERVICE (self));

  self->paused = FALSE;
  gbp_code_index_service_queue_index (self);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PAUSED]);
}

static void
gbp_code_index_service_execute_cb (GObject      *object,
                                   GAsyncResult *result,
                                   gpointer      user_data)
{
  GbpCodeIndexExecutor *executor = (GbpCodeIndexExecutor *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODE_INDEX_EXECUTOR (executor));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!gbp_code_index_executor_execute_finish (executor, result, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

static void
gbp_code_index_service_load_flags_cb (GObject      *object,
                                      GAsyncResult *result,
                                      gpointer      user_data)
{
  GbpCodeIndexPlan *plan = (GbpCodeIndexPlan *)object;
  g_autoptr(GbpCodeIndexExecutor) executor = NULL;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  GbpCodeIndexService *self;
  IdeContext *context;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODE_INDEX_PLAN (plan));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!gbp_code_index_plan_load_flags_finish (plan, result, &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  if (ide_task_return_error_if_cancelled (task))
    IDE_EXIT;

  self = ide_task_get_source_object (task);
  context = ide_task_get_task_data (task);

  g_assert (GBP_IS_CODE_INDEX_SERVICE (self));
  g_assert (IDE_IS_CONTEXT (context));

  executor = gbp_code_index_executor_new (plan);

  gbp_code_index_executor_execute_async (executor,
                                         self->notif,
                                         ide_task_get_cancellable (task),
                                         gbp_code_index_service_execute_cb,
                                         g_object_ref (task));

  IDE_EXIT;
}

static void
gbp_code_index_service_cull_index_cb (GObject      *object,
                                      GAsyncResult *result,
                                      gpointer      user_data)
{
  GbpCodeIndexPlan *plan = (GbpCodeIndexPlan *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  IdeContext *context;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODE_INDEX_PLAN (plan));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!gbp_code_index_plan_cull_indexed_finish (plan, result, &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  if (ide_task_return_error_if_cancelled (task))
    IDE_EXIT;

  context = ide_task_get_task_data (task);
  g_assert (IDE_IS_CONTEXT (context));

  gbp_code_index_plan_load_flags_async (plan,
                                        context,
                                        ide_task_get_cancellable (task),
                                        gbp_code_index_service_load_flags_cb,
                                        g_object_ref (task));

  IDE_EXIT;
}

static void
gbp_code_index_service_populate_cb (GObject      *object,
                                    GAsyncResult *result,
                                    gpointer      user_data)
{
  GbpCodeIndexPlan *plan = (GbpCodeIndexPlan *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  IdeContext *context;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODE_INDEX_PLAN (plan));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!gbp_code_index_plan_populate_finish (plan, result, &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  if (ide_task_return_error_if_cancelled (task))
    IDE_EXIT;

  context = ide_task_get_task_data (task);
  g_assert (IDE_IS_CONTEXT (context));

  gbp_code_index_plan_cull_indexed_async (plan,
                                          context,
                                          ide_task_get_cancellable (task),
                                          gbp_code_index_service_cull_index_cb,
                                          g_object_ref (task));

  IDE_EXIT;
}

static void
gbp_code_index_service_index_async (GbpCodeIndexService *self,
                                    GCancellable        *cancellable,
                                    GAsyncReadyCallback  callback,
                                    gpointer             user_data)
{
  g_autoptr(GbpCodeIndexPlan) plan = NULL;
  g_autoptr(IdeContext) context = NULL;
  g_autoptr(IdeTask) task = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODE_INDEX_SERVICE (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  if (cancellable == NULL)
    g_warning ("Attempt to index without a valid cancellable. This will affect pausibility.");

  self->indexing = TRUE;
  self->needs_indexing = FALSE;

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_code_index_service_index_async);

  if (ide_task_return_error_if_cancelled (task))
    IDE_EXIT;

  context = ide_object_ref_context (IDE_OBJECT (self));
  g_assert (IDE_IS_CONTEXT (context));

  plan = gbp_code_index_plan_new ();

  gbp_code_index_plan_populate_async (plan,
                                      context,
                                      cancellable,
                                      gbp_code_index_service_populate_cb,
                                      g_steal_pointer (&task));

  IDE_EXIT;
}

static gboolean
gbp_code_index_service_index_finish (GbpCodeIndexService  *self,
                                     GAsyncResult         *result,
                                     GError              **error)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODE_INDEX_SERVICE (self));
  g_assert (IDE_IS_TASK (result));

  self->indexing = FALSE;

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

void
gbp_code_index_service_start (GbpCodeIndexService *self)
{
  g_autoptr(IdeContext) context = NULL;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (GBP_IS_CODE_INDEX_SERVICE (self));
  g_return_if_fail (self->started == FALSE);
  g_return_if_fail (!ide_object_in_destruction (IDE_OBJECT (self)));

  self->started = TRUE;

  if (self->paused)
    return;

  gbp_code_index_service_queue_index (self);
}

void
gbp_code_index_service_stop (GbpCodeIndexService *self)
{
  g_autoptr(GCancellable) cancellable = NULL;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (GBP_IS_CODE_INDEX_SERVICE (self));

  if (!self->started)
    return;

  self->started = FALSE;

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);
  g_clear_handle_id (&self->queued_source, g_source_remove);

  if (self->notif)
    {
      ide_notification_withdraw (self->notif);
      g_clear_object (&self->notif);
    }
}

GbpCodeIndexService *
gbp_code_index_service_new (IdeContext *context)
{
  g_return_val_if_fail (IDE_IS_CONTEXT (context), NULL);

  return g_object_new (GBP_TYPE_CODE_INDEX_SERVICE,
                       "parent", context,
                       NULL);
}

gboolean
gbp_code_index_service_get_paused (GbpCodeIndexService *self)
{
  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), FALSE);
  g_return_val_if_fail (GBP_IS_CODE_INDEX_SERVICE (self), FALSE);

  return self->paused;
}

void
gbp_code_index_service_set_paused (GbpCodeIndexService *self,
                                   gboolean             paused)
{
  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (GBP_IS_CODE_INDEX_SERVICE (self));

  paused = !!paused;

  if (paused != self->paused)
    {
      if (paused)
        gbp_code_index_service_pause (self);
      else
        gbp_code_index_service_unpause (self);
    }
}
