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

#include <glib/gi18n.h>

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

static IdePage *
gbp_sysprof_page_create_split (IdePage *page)
{
  GbpSysprofPage *self = (GbpSysprofPage *)page;

  g_assert (GBP_IS_SYSPROF_PAGE (page));
  g_assert (G_IS_FILE (self->file));

  return IDE_PAGE (gbp_sysprof_page_new_for_file (self->file));
}

static void
on_notify_can_replay_cb (GbpSysprofPage *self,
                         GParamSpec     *pspec,
                         SysprofDisplay *display)
{
  g_assert (GBP_IS_SYSPROF_PAGE (self));
  g_assert (SYSPROF_IS_DISPLAY (display));

  panel_widget_action_set_enabled (PANEL_WIDGET (self), "record-again",
                                   sysprof_display_get_can_replay (display));
}

static void
gbp_sysprof_page_set_display (GbpSysprofPage *self,
                              SysprofDisplay *display)
{
  g_assert (GBP_IS_SYSPROF_PAGE (self));
  g_assert (SYSPROF_IS_DISPLAY (display));

  self->display = display;

  g_signal_connect_object (display,
                           "notify::can-replay",
                           G_CALLBACK (on_notify_can_replay_cb),
                           self,
                           G_CONNECT_SWAPPED);
  on_notify_can_replay_cb (self, NULL, display);

  g_object_bind_property (display, "title", self, "title", G_BINDING_SYNC_CREATE);
  gtk_widget_set_hexpand (GTK_WIDGET (display), TRUE);
  gtk_widget_set_vexpand (GTK_WIDGET (display), TRUE);
  ide_page_add_content_widget (IDE_PAGE (self), GTK_WIDGET (display));
}

static void
record_again_action (GtkWidget  *widget,
                     const char *action_name,
                     GVariant   *param)
{
  GbpSysprofPage *self = GBP_SYSPROF_PAGE (widget);
  g_autoptr(PanelPosition) position = NULL;
  GbpSysprofPage *new_page;
  SysprofDisplay *display;
  IdeWorkspace *workspace;

  g_assert (GBP_IS_SYSPROF_PAGE (self));

  if (!sysprof_display_get_can_replay (self->display))
    return;

  if (!(display = sysprof_display_replay (self->display)))
    return;

  new_page = g_object_new (GBP_TYPE_SYSPROF_PAGE, NULL);
  g_set_object (&new_page->file, self->file);
  gbp_sysprof_page_set_display (new_page, display);

  workspace = ide_widget_get_workspace (GTK_WIDGET (self));
  position = ide_page_get_position (IDE_PAGE (self));
  panel_position_set_depth (position, 0);
  ide_workspace_add_page (workspace, IDE_PAGE (new_page), position);
}

static void
save_as_action (GtkWidget  *widget,
                const char *action_name,
                GVariant   *param)
{
  GbpSysprofPage *self = GBP_SYSPROF_PAGE (widget);

  if (sysprof_display_get_can_save (self->display))
    sysprof_display_save (self->display);
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
gbp_sysprof_page_class_init (GbpSysprofPageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  PanelWidgetClass *panel_widget_class = PANEL_WIDGET_CLASS (klass);
  IdePageClass *page_class = IDE_PAGE_CLASS (klass);

  object_class->dispose = gbp_sysprof_page_dispose;
  object_class->get_property = gbp_sysprof_page_get_property;

  page_class->create_split = gbp_sysprof_page_create_split;

  properties [PROP_FILE] =
    g_param_spec_object ("file", NULL, NULL,
                         G_TYPE_FILE,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  panel_widget_class_install_action (panel_widget_class, "save-as", NULL, save_as_action);
  panel_widget_class_install_action (panel_widget_class, "record-again", NULL, record_again_action);
}

static void
gbp_sysprof_page_init (GbpSysprofPage *self)
{
  ide_page_set_menu_id (IDE_PAGE (self), "gbp-sysprof-page-menu");
  panel_widget_set_icon_name (PANEL_WIDGET (self), "builder-profiler-symbolic");
}

GbpSysprofPage *
gbp_sysprof_page_new_for_file (GFile *file)
{
  GbpSysprofPage *self;
  SysprofDisplay *display;

  g_return_val_if_fail (G_IS_FILE (file), NULL);
  g_return_val_if_fail (g_file_is_native (file), NULL);

  self = g_object_new (GBP_TYPE_SYSPROF_PAGE, NULL);
  g_set_object (&self->file, file);
  ide_page_set_can_split (IDE_PAGE (self), TRUE);

  display = SYSPROF_DISPLAY (sysprof_display_new ());
  sysprof_display_open (display, file);

  gbp_sysprof_page_set_display (self, display);

  return self;
}

GbpSysprofPage *
gbp_sysprof_page_new_for_profiler (SysprofProfiler *profiler)
{
  GbpSysprofPage *self;
  SysprofDisplay *display;

  g_return_val_if_fail (SYSPROF_IS_PROFILER (profiler), NULL);

  self = g_object_new (GBP_TYPE_SYSPROF_PAGE, NULL);
  display = SYSPROF_DISPLAY (sysprof_display_new_for_profiler (profiler));
  gbp_sysprof_page_set_display (self, display);

  return self;
}
