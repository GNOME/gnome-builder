/* gbp-sysprof-page.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-sysprof-page"

#include "config.h"

#include <sysprof-ui.h>

#include "gbp-sysprof-page.h"

struct _GbpSysprofPage
{
  IdePage         parent_instance;

  GFile          *file;

  SysprofDisplay *display;
};

enum {
  PROP_0,
  PROP_FILE,
  N_PROPS
};

G_DEFINE_TYPE (GbpSysprofPage, gbp_sysprof_page, IDE_TYPE_PAGE)

static GParamSpec *properties [N_PROPS];

GFile *
gbp_sysprof_page_get_file (GbpSysprofPage *self)
{
  g_return_val_if_fail (GBP_IS_SYSPROF_PAGE (self), NULL);

  return self->file;
}

static void
gbp_sysprof_page_set_file (GbpSysprofPage *self,
                           GFile          *file)
{
  g_assert (GBP_IS_SYSPROF_PAGE (self));
  g_assert (!file || G_IS_FILE (file));
  g_assert (self->file == NULL);

  if (file == NULL)
    return;

  g_set_object (&self->file, file);
  sysprof_display_open (self->display, file);
}

static void
gbp_sysprof_page_dispose (GObject *object)
{
  GbpSysprofPage *self = (GbpSysprofPage *)object;

  g_clear_object (&self->file);

  G_OBJECT_CLASS (gbp_sysprof_page_parent_class)->dispose (object);
}

static void
gbp_sysprof_page_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  GbpSysprofPage *self = GBP_SYSPROF_PAGE (object);

  switch (prop_id)
    {
    case PROP_FILE:
      g_value_set_object (value, gbp_sysprof_page_get_file (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_sysprof_page_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  GbpSysprofPage *self = GBP_SYSPROF_PAGE (object);

  switch (prop_id)
    {
    case PROP_FILE:
      gbp_sysprof_page_set_file (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_sysprof_page_class_init (GbpSysprofPageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gbp_sysprof_page_dispose;
  object_class->get_property = gbp_sysprof_page_get_property;
  object_class->set_property = gbp_sysprof_page_set_property;

  properties [PROP_FILE] =
    g_param_spec_object ("file", NULL, NULL,
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_sysprof_page_init (GbpSysprofPage *self)
{
  self->display = SYSPROF_DISPLAY (sysprof_display_new ());
  g_object_bind_property (self->display, "title", self, "title", 0);
  gtk_widget_set_hexpand (GTK_WIDGET (self->display), TRUE);
  gtk_widget_set_vexpand (GTK_WIDGET (self->display), TRUE);
  ide_page_add_content_widget (IDE_PAGE (self), GTK_WIDGET (self->display));

  panel_widget_set_icon_name (PANEL_WIDGET (self), "builder-profiler-symbolic");
}

GbpSysprofPage *
gbp_sysprof_page_new_for_file (GFile *file)
{
  g_return_val_if_fail (G_IS_FILE (file), NULL);
  g_return_val_if_fail (g_file_is_native (file), NULL);

  return g_object_new (GBP_TYPE_SYSPROF_PAGE,
                       "file", file,
                       NULL);
}
