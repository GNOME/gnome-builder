/* gb-markdown-tab.c
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

#include <glib/gi18n.h>

#include "gb-markdown-preview.h"
#include "gb-markdown-tab.h"

struct _GbMarkdownTabPrivate
{
  GtkTextBuffer     *buffer;
  GbMarkdownPreview *preview;
};

G_DEFINE_TYPE_WITH_PRIVATE (GbMarkdownTab, gb_markdown_tab, GB_TYPE_TAB)

enum {
  PROP_0,
  PROP_BUFFER,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

GbTab *
gb_markdown_tab_new (GtkTextBuffer *buffer)
{
  return g_object_new (GB_TYPE_MARKDOWN_TAB,
                       "buffer", buffer,
                       NULL);
}

static gboolean
remove_tab (gpointer user_data)
{
  GbTab *tab = user_data;

  g_return_val_if_fail (GB_IS_TAB (tab), G_SOURCE_REMOVE);

  gb_tab_close (tab);
  g_object_unref (tab);

  return G_SOURCE_REMOVE;
}

static void
gb_markdown_tab_weak_unref (gpointer  data,
                            GObject  *where_object_was)
{
  GbMarkdownTab *tab = data;

  g_return_if_fail (GB_IS_MARKDOWN_TAB (tab));

  /*
   * Close the tab if we lose our buffer. This will happen with the tab owning
   * the buffer is destroyed. This causes us to destroy with it.
   */
  if (where_object_was == (void *)tab->priv->buffer)
    g_timeout_add (0, remove_tab, g_object_ref (tab));
}

GtkTextBuffer *
gb_markdown_tab_get_buffer (GbMarkdownTab *tab)
{
  g_return_val_if_fail (GB_IS_MARKDOWN_TAB (tab), NULL);

  return tab->priv->buffer;
}

static void
gb_markdown_tab_set_buffer (GbMarkdownTab *tab,
                            GtkTextBuffer *buffer)
{
  g_return_if_fail (GB_IS_MARKDOWN_TAB (tab));
  g_return_if_fail (!buffer || GTK_IS_TEXT_BUFFER (buffer));

  if (tab->priv->buffer != buffer)
    {
      if (tab->priv->buffer)
        {
          g_object_weak_unref (G_OBJECT (tab->priv->buffer),
                               gb_markdown_tab_weak_unref,
                               tab);
          tab->priv->buffer = NULL;
        }

      if (buffer)
        {
          tab->priv->buffer = buffer;
          g_object_weak_ref (G_OBJECT (tab->priv->buffer),
                             gb_markdown_tab_weak_unref,
                             tab);
        }

      gb_markdown_preview_set_buffer (tab->priv->preview, buffer);

      g_object_notify_by_pspec (G_OBJECT (tab), gParamSpecs [PROP_BUFFER]);
    }
}

static void
gb_markdown_tab_finalize (GObject *object)
{
  GbMarkdownTab *tab = GB_MARKDOWN_TAB (object);

  gb_markdown_tab_set_buffer (tab, NULL);

  G_OBJECT_CLASS (gb_markdown_tab_parent_class)->finalize (object);
}

static void
gb_markdown_tab_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  GbMarkdownTab *self = GB_MARKDOWN_TAB (object);

  switch (prop_id)
    {
    case PROP_BUFFER:
      g_value_set_object (value, gb_markdown_tab_get_buffer (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_markdown_tab_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  GbMarkdownTab *self = GB_MARKDOWN_TAB (object);

  switch (prop_id)
    {
    case PROP_BUFFER:
      gb_markdown_tab_set_buffer (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_markdown_tab_class_init (GbMarkdownTabClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gb_markdown_tab_finalize;
  object_class->get_property = gb_markdown_tab_get_property;
  object_class->set_property = gb_markdown_tab_set_property;

  gParamSpecs [PROP_BUFFER] =
    g_param_spec_object ("buffer",
                         _("Buffer"),
                         _("The buffer to monitor."),
                         GTK_TYPE_TEXT_BUFFER,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_BUFFER,
                                   gParamSpecs [PROP_BUFFER]);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/builder/ui/gb-markdown-tab.ui");
  gtk_widget_class_bind_template_child_private (widget_class, GbMarkdownTab, preview);

  g_type_ensure (GB_TYPE_MARKDOWN_PREVIEW);
}

static void
gb_markdown_tab_init (GbMarkdownTab *self)
{
  self->priv = gb_markdown_tab_get_instance_private (self);

  gtk_widget_init_template (GTK_WIDGET (self));
}
