/*
 * folding-buffer-addin.c
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

#include <libide-code.h>
#include <libide-plugins.h>

#include "folding-buffer-addin.h"

struct _FoldingBufferAddin
{
  GObject              parent_instance;

  /* Borrowed pointers */
  IdeBuffer           *buffer;

  /* Owned pointers */
  IdeExtensionAdapter *fold_provider;

  guint                active : 1;
  guint                dirty : 1;
};

enum {
  INVALIDATED,
  N_SIGNALS
};

static guint signals[N_SIGNALS];

static void folding_buffer_addin_list_regions_cb (GObject      *object,
                                                  GAsyncResult *result,
                                                  gpointer      user_data);

static void
folding_buffer_addin_query (FoldingBufferAddin *self)
{
  IdeFoldProvider *fold_provider;

  g_assert (FOLDING_IS_BUFFER_ADDIN (self));

  if (self->buffer == NULL)
    return;

  self->dirty = TRUE;

  if (self->active)
    return;

  if (!(fold_provider = ide_extension_adapter_get_extension (self->fold_provider)))
    return;

  self->active = TRUE;
  self->dirty = FALSE;

  ide_fold_provider_list_regions_async (fold_provider,
                                        self->buffer,
                                        NULL,
                                        folding_buffer_addin_list_regions_cb,
                                        g_object_ref (self));
}

static void
folding_buffer_addin_list_regions_cb (GObject      *object,
                                      GAsyncResult *result,
                                      gpointer      user_data)
{
  IdeFoldProvider *fold_provider = (IdeFoldProvider *)object;
  g_autoptr(FoldingBufferAddin) self = user_data;
  g_autoptr(IdeFoldRegions) regions = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_FOLD_PROVIDER (fold_provider));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (FOLDING_IS_BUFFER_ADDIN (self));

  self->active = FALSE;

  regions = ide_fold_provider_list_regions_finish (fold_provider, result, &error);

  if (self->buffer != NULL)
    ide_buffer_set_fold_regions (self->buffer, regions);

  if (self->dirty)
    folding_buffer_addin_query (self);

  g_signal_emit (self, signals[INVALIDATED], 0);
}

static void
folding_buffer_addin_change_settled (IdeBufferAddin *addin,
                                     IdeBuffer      *buffer)
{
  FoldingBufferAddin *self = (FoldingBufferAddin *)addin;

  g_assert (FOLDING_IS_BUFFER_ADDIN (self));
  g_assert (IDE_IS_BUFFER (buffer));

  folding_buffer_addin_query (self);
}

static void
folding_buffer_addin_file_loaded (IdeBufferAddin *addin,
                                  IdeBuffer      *buffer,
                                  GFile          *file)
{
  FoldingBufferAddin *self = (FoldingBufferAddin *)addin;
  g_autoptr(IdeFoldRegionsBuilder) builder = NULL;
  g_autoptr(IdeFoldRegions) regions = NULL;

  g_assert (FOLDING_IS_BUFFER_ADDIN (self));
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (G_IS_FILE (file));

  // testing
  builder = ide_fold_regions_builder_new (GTK_TEXT_BUFFER (buffer));
  ide_fold_regions_builder_add (builder, 0, -1, 18, -1);
  //ide_fold_regions_builder_add (builder, 9, -1, 12, -1);
  regions = ide_fold_regions_builder_build (builder);
  ide_buffer_set_fold_regions (buffer, regions);
}

static void
folding_buffer_addin_load (IdeBufferAddin *addin,
                           IdeBuffer      *buffer)
{
  FoldingBufferAddin *self = (FoldingBufferAddin *)addin;
  IdeObjectBox *box;
  const char *language_id;

  g_assert (FOLDING_IS_BUFFER_ADDIN (self));
  g_assert (IDE_IS_BUFFER (buffer));

  self->buffer = buffer;

  language_id = ide_buffer_get_language_id (buffer);
  box = ide_object_box_from_object (G_OBJECT (buffer));
  self->fold_provider = ide_extension_adapter_new (IDE_OBJECT (box),
                                                   peas_engine_get_default (),
                                                   IDE_TYPE_FOLD_PROVIDER,
                                                   "Fold-Provider-Lanuages",
                                                   language_id);

  g_object_bind_property (self->buffer, "language-id",
                          self->fold_provider, "value",
                          G_BINDING_SYNC_CREATE);
}

static void
folding_buffer_addin_unload (IdeBufferAddin *addin,
                             IdeBuffer      *buffer)
{
  FoldingBufferAddin *self = (FoldingBufferAddin *)addin;

  g_assert (FOLDING_IS_BUFFER_ADDIN (self));
  g_assert (IDE_IS_BUFFER (buffer));

  ide_clear_and_destroy_object (&self->fold_provider);

  self->buffer = NULL;
}

static void
buffer_addin_iface_init (IdeBufferAddinInterface *iface)
{
  iface->change_settled = folding_buffer_addin_change_settled;
  iface->file_loaded = folding_buffer_addin_file_loaded;
  iface->load = folding_buffer_addin_load;
  iface->unload = folding_buffer_addin_unload;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (FoldingBufferAddin, folding_buffer_addin, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_BUFFER_ADDIN, buffer_addin_iface_init))

static void
folding_buffer_addin_class_init (FoldingBufferAddinClass *klass)
{
  signals[INVALIDATED] =
    g_signal_new ("invalidated",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);
}

static void
folding_buffer_addin_init (FoldingBufferAddin *self)
{
}

IdeFoldRegions *
folding_buffer_addin_get_fold_regions (FoldingBufferAddin *self)
{
  g_return_val_if_fail (FOLDING_IS_BUFFER_ADDIN (self), NULL);

  if (self->buffer == NULL)
    return NULL;

  return ide_buffer_get_fold_regions (self->buffer);
}
