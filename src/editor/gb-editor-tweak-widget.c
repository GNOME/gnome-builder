/* gb-editor-tweak-widget.c
 *
 * Copyright (C) 2014 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "editor-tweak"

#include <glib/gi18n.h>

#include "gb-editor-tweak-widget.h"
#include "gb-widget.h"

struct _GbEditorTweakWidgetPrivate
{
  GtkSearchEntry *entry;
  GtkListBox     *list_box;
};

G_DEFINE_TYPE_WITH_PRIVATE (GbEditorTweakWidget, gb_editor_tweak_widget,
                            GTK_TYPE_BIN)

enum {
  PROP_0,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

GtkWidget *
gb_editor_tweak_widget_new (void)
{
  return g_object_new (GB_TYPE_EDITOR_TWEAK_WIDGET, NULL);
}

static void
gb_editor_tweak_widget_finalize (GObject *object)
{
  G_OBJECT_CLASS (gb_editor_tweak_widget_parent_class)->finalize (object);
}

static void
gb_editor_tweak_widget_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  GbEditorTweakWidget *self = GB_EDITOR_TWEAK_WIDGET (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_editor_tweak_widget_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  GbEditorTweakWidget *self = GB_EDITOR_TWEAK_WIDGET (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_editor_tweak_widget_class_init (GbEditorTweakWidgetClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gb_editor_tweak_widget_finalize;
  object_class->get_property = gb_editor_tweak_widget_get_property;
  object_class->set_property = gb_editor_tweak_widget_set_property;

  GB_WIDGET_CLASS_TEMPLATE (widget_class, "gb-editor-tweak-widget.ui");
  GB_WIDGET_CLASS_BIND (widget_class, GbEditorTweakWidget, entry);
  GB_WIDGET_CLASS_BIND (widget_class, GbEditorTweakWidget, list_box);
}

static void
gb_editor_tweak_widget_init (GbEditorTweakWidget *self)
{
  self->priv = gb_editor_tweak_widget_get_instance_private (self);

  gtk_widget_init_template (GTK_WIDGET (self));
}
