/* ide-source-view.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define G_LOG_DOMAIN "ide-source-view"

#include <glib/gi18n.h>

#include "ide-buffer.h"
#include "ide-file.h"
#include "ide-file-settings.h"
#include "ide-highlighter.h"
#include "ide-indenter.h"
#include "ide-language.h"
#include "ide-pango.h"
#include "ide-source-view.h"

#define DEFAULT_FONT_DESC "Monospace 11"

typedef struct
{
  IdeBuffer            *buffer;
  GtkCssProvider       *css_provider;
  PangoFontDescription *font_desc;
} IdeSourceViewPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (IdeSourceView, ide_source_view, GTK_SOURCE_TYPE_VIEW)

enum {
  PROP_0,
  PROP_FONT_NAME,
  PROP_FONT_DESC,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

static void
ide_source_view__buffer_notify_file_cb (IdeSourceView *self,
                                        GParamSpec    *pspec,
                                        IdeBuffer     *buffer)
{
  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (IDE_IS_BUFFER (buffer));
}

static void
ide_source_view__buffer_notify_language_cb (IdeSourceView *self,
                                            GParamSpec    *pspec,
                                            IdeBuffer     *buffer)
{
  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (IDE_IS_BUFFER (buffer));
}

static void
ide_source_view__buffer_changed_cb (IdeSourceView *self,
                                    IdeBuffer     *buffer)
{
  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (IDE_IS_BUFFER (buffer));

}

static void
ide_source_view_rebuild_css (IdeSourceView *self)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_return_if_fail (IDE_IS_SOURCE_VIEW (self));

  if (!priv->css_provider)
    {
      GtkStyleContext *style_context;

      priv->css_provider = gtk_css_provider_new ();
      style_context = gtk_widget_get_style_context (GTK_WIDGET (self));
      gtk_style_context_add_provider (style_context,
                                      GTK_STYLE_PROVIDER (priv->css_provider),
                                      GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    }

  if (priv->font_desc)
    {
      g_autofree gchar *str = NULL;
      g_autofree gchar *css = NULL;

      str = ide_pango_font_description_to_css (priv->font_desc);
      css = g_strdup_printf ("IdeSourceView { %s }", str ?: "");
      gtk_css_provider_load_from_data (priv->css_provider, css, -1, NULL);
    }
}

static void
ide_source_view_connect_buffer (IdeSourceView *self,
                                IdeBuffer     *buffer)
{
  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (IDE_IS_BUFFER (buffer));

  g_signal_connect_object (buffer,
                           "changed",
                           G_CALLBACK (ide_source_view__buffer_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (buffer,
                           "notify::file",
                           G_CALLBACK (ide_source_view__buffer_notify_file_cb),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (buffer,
                           "notify::language",
                           G_CALLBACK (ide_source_view__buffer_notify_language_cb),
                           self,
                           G_CONNECT_SWAPPED);
}

static void
ide_source_view_disconnect_buffer (IdeSourceView *self,
                                   IdeBuffer     *buffer)
{
  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (IDE_IS_BUFFER (buffer));

  g_signal_handlers_disconnect_by_func (buffer,
                                        G_CALLBACK (ide_source_view__buffer_notify_file_cb),
                                        self);

  g_signal_handlers_disconnect_by_func (buffer,
                                        G_CALLBACK (ide_source_view__buffer_notify_language_cb),
                                        self);
}

static void
ide_source_view_notify_buffer (IdeSourceView *self,
                               GParamSpec    *pspec,
                               gpointer       user_data)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  GtkTextBuffer *buffer;

  g_assert (IDE_IS_SOURCE_VIEW (self));

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));

  if (priv->buffer != (IdeBuffer *)buffer)
    {
      if (priv->buffer != NULL)
        {
          ide_source_view_disconnect_buffer (self, priv->buffer);
          g_clear_object (&priv->buffer);
        }

      /*
       * Only enable IdeSourceView features if this is an IdeBuffer.
       * Ignore for GtkSourceBuffer, and GtkTextBuffer.
       */
      if (IDE_IS_BUFFER (buffer))
        {
          priv->buffer = g_object_ref (buffer);
          ide_source_view_connect_buffer (self, priv->buffer);
        }
    }
}

static void
ide_source_view_dispose (GObject *object)
{
  IdeSourceView *self = (IdeSourceView *)object;
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  if (priv->buffer)
    {
      ide_source_view_disconnect_buffer (self, priv->buffer);
      g_clear_object (&priv->buffer);
    }

  g_clear_pointer (&priv->font_desc, pango_font_description_free);

  G_OBJECT_CLASS (ide_source_view_parent_class)->dispose (object);
}

static void
ide_source_view_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  IdeSourceView *self = IDE_SOURCE_VIEW (object);

  switch (prop_id)
    {
    case PROP_FONT_DESC:
      g_value_set_boxed (value, ide_source_view_get_font_desc (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_source_view_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  IdeSourceView *self = IDE_SOURCE_VIEW (object);

  switch (prop_id)
    {
    case PROP_FONT_NAME:
      ide_source_view_set_font_name (self, g_value_get_string (value));
      break;

    case PROP_FONT_DESC:
      ide_source_view_set_font_desc (self, g_value_get_boxed (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_source_view_class_init (IdeSourceViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_source_view_dispose;
  object_class->get_property = ide_source_view_get_property;
  object_class->set_property = ide_source_view_set_property;

  gParamSpecs [PROP_FONT_DESC] =
    g_param_spec_boxed ("font-desc",
                        _("Font Description"),
                        _("The Pango font description to use for rendering source."),
                        PANGO_TYPE_FONT_DESCRIPTION,
                        (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_FONT_DESC,
                                   gParamSpecs [PROP_FONT_DESC]);

  gParamSpecs [PROP_FONT_NAME] =
    g_param_spec_string ("font-name",
                         _("Font Name"),
                         _("The pango font name ot use for rendering source."),
                         "Monospace",
                         (G_PARAM_WRITABLE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_FONT_NAME,
                                   gParamSpecs [PROP_FONT_NAME]);
}

static void
ide_source_view_init (IdeSourceView *self)
{
  g_signal_connect (self, "notify::buffer", G_CALLBACK (ide_source_view_notify_buffer), NULL);
}

const PangoFontDescription *
ide_source_view_get_font_desc (IdeSourceView *self)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_SOURCE_VIEW (self), NULL);

  return priv->font_desc;
}

void
ide_source_view_set_font_desc (IdeSourceView              *self,
                               const PangoFontDescription *font_desc)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_return_if_fail (IDE_IS_SOURCE_VIEW (self));

  if (font_desc != priv->font_desc)
    {
      g_clear_pointer (&priv->font_desc, pango_font_description_free);

      if (font_desc)
        priv->font_desc = pango_font_description_copy (font_desc);
      else
        priv->font_desc = pango_font_description_from_string (DEFAULT_FONT_DESC);

      ide_source_view_rebuild_css (self);
    }
}

void
ide_source_view_set_font_name (IdeSourceView *self,
                               const gchar   *font_name)
{
  PangoFontDescription *font_desc = NULL;

  g_return_if_fail (IDE_IS_SOURCE_VIEW (self));

  if (font_name)
    font_desc = pango_font_description_from_string (font_name);
  ide_source_view_set_font_desc (self, font_desc);
  if (font_desc)
    pango_font_description_free (font_desc);
}
