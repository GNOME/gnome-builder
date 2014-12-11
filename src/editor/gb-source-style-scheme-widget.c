/* gb-source-style-scheme-widget.c
 *
 * Copyright (C) 2014 Christian Hergert <christian@hergert.me>
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

#include <glib/gi18n.h>
#include <gtksourceview/gtksource.h>

#include "gb-source-style-scheme-widget.h"

struct _GbSourceStyleSchemeWidgetPrivate
{
  GtkBox            *vbox;
  GtkListBox        *list_box;
  GtkScrolledWindow *scroller;
};

G_DEFINE_TYPE_WITH_PRIVATE (GbSourceStyleSchemeWidget,
                            gb_source_style_scheme_widget,
                            GTK_TYPE_BIN)

enum {
  PROP_0,
  PROP_STYLE_SCHEME_NAME,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

GtkWidget *
gb_source_style_scheme_widget_new (void)
{
  return g_object_new (GB_TYPE_SOURCE_STYLE_SCHEME_WIDGET, NULL);
}

const gchar *
gb_source_style_scheme_widget_get_style_scheme_name (GbSourceStyleSchemeWidget *widget)
{
  GtkListBoxRow *row;

  g_return_val_if_fail (GB_IS_SOURCE_STYLE_SCHEME_WIDGET (widget), NULL);

  row = gtk_list_box_get_selected_row (widget->priv->list_box);

  if (row)
    return g_object_get_data (G_OBJECT (row), "scheme_id");

  return NULL;
}

void
gb_source_style_scheme_widget_set_style_scheme_name (GbSourceStyleSchemeWidget *widget,
                                                     const gchar               *style_scheme_name)
{
  GList *children;
  GList *iter;

  g_return_if_fail (GB_IS_SOURCE_STYLE_SCHEME_WIDGET (widget));

  children = gtk_container_get_children (GTK_CONTAINER (widget->priv->list_box));

  for (iter = children; iter; iter = iter->next)
    {
      const gchar *cur;

      cur = g_object_get_data (G_OBJECT (iter->data), "scheme_id");

      if (0 == g_strcmp0 (cur, style_scheme_name))
        {
          gtk_list_box_select_row (widget->priv->list_box, iter->data);
          break;
        }
    }

  g_list_free (children);
}

static GtkListBoxRow *
make_row (GtkSourceStyleScheme *scheme,
          GtkSourceLanguage    *language)
{
  GtkListBoxRow *row;
  GtkSourceBuffer *buffer;
  GtkSourceView *view;
  const gchar *scheme_id;
  gchar *text;

  g_return_val_if_fail (GTK_SOURCE_IS_STYLE_SCHEME (scheme), NULL);
  g_return_val_if_fail (GTK_SOURCE_IS_LANGUAGE (language), NULL);

  row = g_object_new (GTK_TYPE_LIST_BOX_ROW,
                      "visible", TRUE,
                      NULL);

  scheme_id = gtk_source_style_scheme_get_id (scheme);
  g_object_set_data_full (G_OBJECT (row), "scheme_id",
                          g_strdup (scheme_id), g_free);

  buffer = g_object_new (GTK_SOURCE_TYPE_BUFFER,
                         "highlight-matching-brackets", FALSE,
                         "language", language,
                         "style-scheme", scheme,
                         NULL);

  text = g_strdup_printf ("/* %s */\n#include <gnome-builder.h>",
                          gtk_source_style_scheme_get_name (scheme));
  gtk_text_buffer_set_text (GTK_TEXT_BUFFER (buffer), text, -1);

  view = g_object_new (GTK_SOURCE_TYPE_VIEW,
                       "buffer", buffer,
                       "can-focus", FALSE,
                       "cursor-visible", FALSE,
                       "editable", FALSE,
                       "visible", TRUE,
                       "show-line-numbers", TRUE,
                       "right-margin-position", 30,
                       "show-right-margin", TRUE,
                       NULL);
  gtk_container_add (GTK_CONTAINER (row), GTK_WIDGET (view));

  return row;
}

static void
gb_source_style_scheme_widget_populate (GbSourceStyleSchemeWidget *widget)
{
  GtkSourceLanguageManager *lm;
  GtkSourceLanguage *lang;
  GtkSourceStyleSchemeManager *manager;
  const gchar * const *scheme_ids;
  guint i;

  g_assert (GB_IS_SOURCE_STYLE_SCHEME_WIDGET (widget));

  manager = gtk_source_style_scheme_manager_get_default ();
  scheme_ids = gtk_source_style_scheme_manager_get_scheme_ids (manager);

  lm = gtk_source_language_manager_get_default ();
  lang = gtk_source_language_manager_get_language (lm, "c");

  for (i = 0; scheme_ids [i]; i++)
    {
      GtkListBoxRow *row;
      GtkSourceStyleScheme *scheme;

      scheme = gtk_source_style_scheme_manager_get_scheme (manager, scheme_ids [i]);
      row = make_row (scheme, lang);
      gtk_container_add (GTK_CONTAINER (widget->priv->list_box), GTK_WIDGET (row));
    }
}

static void
gb_source_style_scheme_widget_constructed (GObject *object)
{
  G_OBJECT_CLASS (gb_source_style_scheme_widget_parent_class)->constructed (object);

  gb_source_style_scheme_widget_populate (GB_SOURCE_STYLE_SCHEME_WIDGET (object));
}

static void
gb_source_style_scheme_widget_get_property (GObject    *object,
                                            guint       prop_id,
                                            GValue     *value,
                                            GParamSpec *pspec)
{
  GbSourceStyleSchemeWidget *self = GB_SOURCE_STYLE_SCHEME_WIDGET (object);

  switch (prop_id)
    {
    case PROP_STYLE_SCHEME_NAME:
      g_value_set_string (value,
                          gb_source_style_scheme_widget_get_style_scheme_name (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_source_style_scheme_widget_set_property (GObject      *object,
                                            guint         prop_id,
                                            const GValue *value,
                                            GParamSpec   *pspec)
{
  GbSourceStyleSchemeWidget *self = GB_SOURCE_STYLE_SCHEME_WIDGET (object);

  switch (prop_id)
    {
    case PROP_STYLE_SCHEME_NAME:
      gb_source_style_scheme_widget_set_style_scheme_name (self,
                                                           g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_source_style_scheme_widget_class_init (GbSourceStyleSchemeWidgetClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = gb_source_style_scheme_widget_constructed;
  object_class->get_property = gb_source_style_scheme_widget_get_property;
  object_class->set_property = gb_source_style_scheme_widget_set_property;

  gParamSpecs [PROP_STYLE_SCHEME_NAME] =
    g_param_spec_string ("style-scheme-name",
                         _("Style Scheme Name"),
                         _("The style scheme name that is selected."),
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_STYLE_SCHEME_NAME,
                                   gParamSpecs [PROP_STYLE_SCHEME_NAME]);
}

static void
gb_source_style_scheme_widget_init (GbSourceStyleSchemeWidget *self)
{
  self->priv = gb_source_style_scheme_widget_get_instance_private (self);

  self->priv->vbox = g_object_new (GTK_TYPE_BOX,
                                   "orientation", GTK_ORIENTATION_VERTICAL,
                                   "spacing", 3,
                                   "visible", TRUE,
                                   NULL);
  gtk_container_add (GTK_CONTAINER (self), GTK_WIDGET (self->priv->vbox));

  self->priv->scroller = g_object_new (GTK_TYPE_SCROLLED_WINDOW,
                                       "visible", TRUE,
                                       "vexpand", TRUE,
                                       NULL);
  gtk_container_add (GTK_CONTAINER (self->priv->vbox),
                     GTK_WIDGET (self->priv->scroller));

  self->priv->list_box = g_object_new (GTK_TYPE_LIST_BOX,
                                       "visible", TRUE,
                                       NULL);
  gtk_container_add (GTK_CONTAINER (self->priv->scroller),
                     GTK_WIDGET (self->priv->list_box));
}
