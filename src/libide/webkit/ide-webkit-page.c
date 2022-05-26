/* ide-webkit-page.c
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

#define G_LOG_DOMAIN "ide-webkit-page"

#include "config.h"

#include <webkit2/webkit2.h>

#include "ide-webkit-page.h"

typedef struct
{
  WebKitWebView *webview;
} IdeWebkitPagePrivate;

enum {
  PROP_0,
  N_PROPS
};

G_DEFINE_TYPE_WITH_PRIVATE (IdeWebkitPage, ide_webkit_page, IDE_TYPE_PAGE)

static GParamSpec *properties [N_PROPS];

static void
ide_webkit_page_dispose (GObject *object)
{
  G_OBJECT_CLASS (ide_webkit_page_parent_class)->dispose (object);
}

static void
ide_webkit_page_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  IdeWebkitPage *self = IDE_WEBKIT_PAGE (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_webkit_page_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  IdeWebkitPage *self = IDE_WEBKIT_PAGE (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_webkit_page_class_init (IdeWebkitPageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = ide_webkit_page_dispose;
  object_class->get_property = ide_webkit_page_get_property;
  object_class->set_property = ide_webkit_page_set_property;

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/webkit/ide-webkit-page.ui");
  gtk_widget_class_bind_template_child_private (widget_class, IdeWebkitPage, webview);

  g_type_ensure (WEBKIT_TYPE_WEB_VIEW);
}

static void
ide_webkit_page_init (IdeWebkitPage *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

IdeWebkitPage *
ide_webkit_page_new (void)
{
  return g_object_new (IDE_TYPE_WEBKIT_PAGE, NULL);
}
