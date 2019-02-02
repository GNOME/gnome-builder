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

#include "gbp-code-index-plan.h"
#include "gbp-code-index-service.h"

struct _GbpCodeIndexService
{
  IdeObject parent_instance;

  GbpCodeIndexPlan *plan;

  guint started : 1;
  guint paused : 1;
};

enum {
  PROP_0,
  PROP_PAUSED,
  N_PROPS
};

G_DEFINE_TYPE (GbpCodeIndexService, gbp_code_index_service, IDE_TYPE_OBJECT)

static GParamSpec *properties [N_PROPS];

static gchar *
gbp_code_index_service_repr (IdeObject *object)
{
  GbpCodeIndexService *self = (GbpCodeIndexService *)object;

  return g_strdup_printf ("%s started=%d paused=%d",
                          G_OBJECT_TYPE_NAME (self), self->started, self->paused);
}

static void
gbp_code_index_service_finalize (GObject *object)
{
  GbpCodeIndexService *self = (GbpCodeIndexService *)object;

  G_OBJECT_CLASS (gbp_code_index_service_parent_class)->finalize (object);
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

  object_class->finalize = gbp_code_index_service_finalize;
  object_class->get_property = gbp_code_index_service_get_property;
  object_class->set_property = gbp_code_index_service_set_property;

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
gbp_code_index_service_pause (GbpCodeIndexService *self)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODE_INDEX_SERVICE (self));

  self->paused = TRUE;
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PAUSED]);
}

static void
gbp_code_index_service_unpause (GbpCodeIndexService *self)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODE_INDEX_SERVICE (self));

  self->paused = FALSE;
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PAUSED]);
}

static void
gbp_code_index_service_cull_index_cb (GObject      *object,
                                      GAsyncResult *result,
                                      gpointer      user_data)
{
  GbpCodeIndexPlan *plan = (GbpCodeIndexPlan *)object;
  g_autoptr(GbpCodeIndexService) self = user_data;
  g_autoptr(IdeContext) context = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODE_INDEX_PLAN (plan));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (GBP_IS_CODE_INDEX_SERVICE (self));

  if (!gbp_code_index_plan_cull_indexed_finish (plan, result, &error))
    {
      g_warning ("Failed to cull operations from plan: %s", error->message);
      if (plan == self->plan)
        g_clear_object (&self->plan);
      return;
    }

}

static void
gbp_code_index_service_populate_cb (GObject      *object,
                                    GAsyncResult *result,
                                    gpointer      user_data)
{
  GbpCodeIndexPlan *plan = (GbpCodeIndexPlan *)object;
  g_autoptr(GbpCodeIndexService) self = user_data;
  g_autoptr(IdeContext) context = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODE_INDEX_PLAN (plan));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (GBP_IS_CODE_INDEX_SERVICE (self));

  if (!gbp_code_index_plan_populate_finish (plan, result, &error))
    {
      g_warning ("Failed to populate code-index: %s", error->message);
      if (plan == self->plan)
        g_clear_object (&self->plan);
      return;
    }

  if (!(context = ide_object_ref_context (IDE_OBJECT (self))))
    return;

  gbp_code_index_plan_cull_indexed_async (plan,
                                          context,
                                          self->cancellable,
                                          gbp_code_index_service_cull_index_cb,
                                          g_object_ref (self));
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

  self->plan = gbp_code_index_plan_new ();

  context = ide_object_ref_context (IDE_OBJECT (self));

  gbp_code_index_plan_populate_async (self->plan,
                                      context,
                                      self->cancellable,
                                      gbp_code_index_service_populate_cb,
                                      g_object_ref (self));
}

void
gbp_code_index_service_stop (GbpCodeIndexService *self)
{
  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (GBP_IS_CODE_INDEX_SERVICE (self));
  g_return_if_fail (self->started == TRUE);
  g_return_if_fail (!ide_object_in_destruction (IDE_OBJECT (self)));

  self->started = FALSE;
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
