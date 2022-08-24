/* ide-tweaks-info.c
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

#define G_LOG_DOMAIN "ide-tweaks-info"

#include "config.h"

#include <adwaita.h>

#include "ide-tweaks-info.h"

struct _IdeTweaksInfo
{
  IdeTweaksWidget parent_instance;
  char *title;
  char *value;
};

enum {
  PROP_0,
  PROP_TITLE,
  PROP_VALUE,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (IdeTweaksInfo, ide_tweaks_info, IDE_TYPE_TWEAKS_WIDGET)

static GParamSpec *properties [N_PROPS];

static GtkWidget *
ide_tweaks_info_create_for_item (IdeTweaksWidget *widget,
                                 IdeTweaksItem   *for_item)
{
  IdeTweaksInfo *info = IDE_TWEAKS_INFO (for_item);
  AdwActionRow *row;
  GtkLabel *value;

  value = g_object_new (GTK_TYPE_LABEL,
                        "xalign", 1.f,
                        "hexpand", TRUE,
                        "use-markup", FALSE,
                        "label", info->value,
                        "selectable", TRUE,
                        "wrap", TRUE,
                        NULL);
  row = g_object_new (ADW_TYPE_ACTION_ROW,
                      "activatable", FALSE,
                      "title", info->title,
                      NULL);
  adw_action_row_add_suffix (row, GTK_WIDGET (value));

  return GTK_WIDGET (row);
}

static void
ide_tweaks_info_dispose (GObject *object)
{
  IdeTweaksInfo *self = (IdeTweaksInfo *)object;

  g_clear_pointer (&self->title, g_free);
  g_clear_pointer (&self->value, g_free);

  G_OBJECT_CLASS (ide_tweaks_info_parent_class)->dispose (object);
}

static void
ide_tweaks_info_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  IdeTweaksInfo *self = IDE_TWEAKS_INFO (object);

  switch (prop_id)
    {
    IDE_GET_PROPERTY_STRING (ide_tweaks_info, title, TITLE);
    IDE_GET_PROPERTY_STRING (ide_tweaks_info, value, VALUE);
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_tweaks_info_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  IdeTweaksInfo *self = IDE_TWEAKS_INFO (object);

  switch (prop_id)
    {
    IDE_SET_PROPERTY_STRING (ide_tweaks_info, title, TITLE);
    IDE_SET_PROPERTY_STRING (ide_tweaks_info, value, VALUE);
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_tweaks_info_class_init (IdeTweaksInfoClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeTweaksWidgetClass *tweaks_widget_class = IDE_TWEAKS_WIDGET_CLASS (klass);

  object_class->dispose = ide_tweaks_info_dispose;
  object_class->get_property = ide_tweaks_info_get_property;
  object_class->set_property = ide_tweaks_info_set_property;

  tweaks_widget_class->create_for_item = ide_tweaks_info_create_for_item;

  IDE_DEFINE_STRING_PROPERTY ("title", NULL, G_PARAM_READWRITE, TITLE);
  IDE_DEFINE_STRING_PROPERTY ("value", NULL, G_PARAM_READWRITE, VALUE);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_tweaks_info_init (IdeTweaksInfo *self)
{
}

IdeTweaksInfo *
ide_tweaks_info_new (void)
{
  return g_object_new (IDE_TYPE_TWEAKS_INFO, NULL);
}

IDE_DEFINE_STRING_GETTER (ide_tweaks_info, IdeTweaksInfo, IDE_TYPE_TWEAKS_INFO, title)
IDE_DEFINE_STRING_SETTER (ide_tweaks_info, IdeTweaksInfo, IDE_TYPE_TWEAKS_INFO, title, TITLE)
IDE_DEFINE_STRING_GETTER (ide_tweaks_info, IdeTweaksInfo, IDE_TYPE_TWEAKS_INFO, value)
IDE_DEFINE_STRING_SETTER (ide_tweaks_info, IdeTweaksInfo, IDE_TYPE_TWEAKS_INFO, value, VALUE)
