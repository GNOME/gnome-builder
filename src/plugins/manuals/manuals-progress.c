/*
 * manuals-progress.c
 *
 * Copyright 2024 Christian Hergert <chergert@redhat.com>
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

#include "config.h"

#include <libdex.h>

#include "manuals-progress.h"

struct _ManualsProgress
{
  GObject parent_instance;
  GPtrArray *jobs;
};

enum {
  PROP_0,
  PROP_FRACTION,
  PROP_N_ITEMS,
  N_PROPS
};

enum {
  OP_ADDED = 0,
  OP_REMOVED = 1,
  OP_FRACTION = 2,
  N_OPS
};

static guint
manuals_progress_get_n_items (GListModel *model)
{
  return MANUALS_PROGRESS (model)->jobs->len;
}

static GType
manuals_progress_get_item_type (GListModel *model)
{
  return MANUALS_TYPE_JOB;
}

static gpointer
manuals_progress_get_item (GListModel *model,
                           guint       position)
{
  ManualsProgress *self = MANUALS_PROGRESS (model);

  if (position < self->jobs->len)
    return g_object_ref (g_ptr_array_index (self->jobs, position));

  return NULL;
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_n_items = manuals_progress_get_n_items;
  iface->get_item_type = manuals_progress_get_item_type;
  iface->get_item = manuals_progress_get_item;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (ManualsProgress, manuals_progress, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

static GParamSpec *properties[N_PROPS];

typedef struct _NotifyInMain
{
  ManualsProgress *self;
  ManualsJob *job;
  guint op : 2;
} NotifyInMain;

static void
notify_in_main_cb (gpointer user_data)
{
  NotifyInMain *state = user_data;

  g_assert (state != NULL);
  g_assert (MANUALS_IS_PROGRESS (state->self));
  g_assert (MANUALS_IS_JOB (state->job));

  if (state->op == OP_ADDED)
    {
      g_ptr_array_add (state->self->jobs, g_object_ref (state->job));
      g_list_model_items_changed (G_LIST_MODEL (state->self),
                                  state->self->jobs->len - 1,
                                  0, 1);
      if (state->self->jobs->len == 1)
        g_object_notify_by_pspec (G_OBJECT (state->self), properties[PROP_N_ITEMS]);
    }
  else if (state->op == OP_REMOVED)
    {
      guint pos = 0;

      if (g_ptr_array_find (state->self->jobs, state->job, &pos))
        {
          g_ptr_array_remove_index (state->self->jobs, pos);
          g_list_model_items_changed (G_LIST_MODEL (state->self), pos, 1, 0);
          if (state->self->jobs->len == 0)
            g_object_notify_by_pspec (G_OBJECT (state->self), properties[PROP_N_ITEMS]);
        }
    }
  else if (state->op == OP_FRACTION) { /* Do nothing */ }

  g_object_notify_by_pspec (G_OBJECT (state->self),
                            properties[PROP_FRACTION]);

  g_clear_object (&state->self);
  g_clear_object (&state->job);
  g_free (state);
}

static void
notify_in_main (ManualsProgress *self,
                ManualsJob      *job,
                guint            op)
{
  NotifyInMain *state;

  g_assert (MANUALS_IS_PROGRESS (self));
  g_assert (MANUALS_IS_JOB (job));
  g_assert (op < N_OPS);

  state = g_new0 (NotifyInMain, 1);
  state->self = g_object_ref (self);
  state->job = g_object_ref (job);
  state->op = op;

  dex_scheduler_push (dex_scheduler_get_default (),
                      notify_in_main_cb,
                      state);
}

static void
manuals_progress_finalize (GObject *object)
{
  ManualsProgress *self = (ManualsProgress *)object;

  g_clear_pointer (&self->jobs, g_ptr_array_unref);

  G_OBJECT_CLASS (manuals_progress_parent_class)->finalize (object);
}

static void
manuals_progress_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  ManualsProgress *self = MANUALS_PROGRESS (object);

  switch (prop_id)
    {
    case PROP_FRACTION:
      g_value_set_double (value, manuals_progress_get_fraction (self));
      break;

    case PROP_N_ITEMS:
      g_value_set_uint (value, self->jobs->len);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
manuals_progress_class_init (ManualsProgressClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = manuals_progress_finalize;
  object_class->get_property = manuals_progress_get_property;

  properties[PROP_FRACTION] =
    g_param_spec_double ("fraction", NULL, NULL,
                         0, 1, 0,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_N_ITEMS] =
    g_param_spec_uint ("n-items", NULL, NULL,
                       0, G_MAXUINT-1, 0,
                       (G_PARAM_READABLE |
                        G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
manuals_progress_init (ManualsProgress *self)
{
  self->jobs = g_ptr_array_new_with_free_func (g_object_unref);
}

ManualsProgress *
manuals_progress_new (void)
{
  return g_object_new (MANUALS_TYPE_PROGRESS, NULL);
}

static void
manuals_progress_job_completed_cb (ManualsProgress *self,
                                   ManualsJob      *job)
{
  g_assert (MANUALS_IS_PROGRESS (self));
  g_assert (MANUALS_IS_JOB (job));

  notify_in_main (self, job, OP_REMOVED);
}

static void
manuals_progress_job_notify_fraction_cb (ManualsProgress *self,
                                         GParamSpec      *pspec,
                                         ManualsJob      *job)
{
  g_assert (MANUALS_IS_PROGRESS (self));
  g_assert (MANUALS_IS_JOB (job));

  notify_in_main (self, job, OP_FRACTION);
}

ManualsJob *
manuals_progress_begin_job (ManualsProgress *self)
{
  ManualsJob *job;

  g_return_val_if_fail (MANUALS_IS_PROGRESS (self), NULL);

  job = g_object_new (MANUALS_TYPE_JOB, NULL);
  g_signal_connect_object (job,
                           "notify::fraction",
                           G_CALLBACK (manuals_progress_job_notify_fraction_cb),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (job,
                           "completed",
                           G_CALLBACK (manuals_progress_job_completed_cb),
                           self,
                           G_CONNECT_SWAPPED);
  notify_in_main (self, job, OP_ADDED);
  return job;
}

double
manuals_progress_get_fraction (ManualsProgress *self)
{
  double total = 0;

  g_return_val_if_fail (MANUALS_IS_PROGRESS (self), 0);

  if (self->jobs->len == 0)
    return 0;

  for (guint i = 0; i < self->jobs->len; i++)
    {
      ManualsJob *job = g_ptr_array_index (self->jobs, i);
      total += CLAMP (manuals_job_get_fraction (job), 0, 1);
    }

  return total / (double)self->jobs->len;
}
