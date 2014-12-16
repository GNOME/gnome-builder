/* gb-search-display.c
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

#define G_LOG_DOMAIN "search-display"

#include <glib/gi18n.h>

#include "gb-log.h"
#include "gb-search-display.h"
#include "gb-search-provider.h"
#include "gb-search-result.h"

struct _GbSearchDisplayPrivate
{
  /* References owned by widget */
  GbSearchContext *context;

  /* References owned by Gtk template */
  GtkListBox      *list_box;
};

G_DEFINE_TYPE_WITH_PRIVATE (GbSearchDisplay, gb_search_display, GTK_TYPE_BIN)

enum {
  PROP_0,
  PROP_CONTEXT,
  LAST_PROP
};

enum {
  RESULT_ACTIVATED,
  LAST_SIGNAL
};

static GParamSpec *gParamSpecs [LAST_PROP];
static guint       gSignals [LAST_SIGNAL];

GtkWidget *
gb_search_display_new (void)
{
  return g_object_new (GB_TYPE_SEARCH_DISPLAY, NULL);
}

static void
gb_search_display_results_added (GbSearchDisplay  *display,
                                 GbSearchProvider *provider,
                                 GList            *results,
                                 gboolean          finished)
{
  GList *iter;

  ENTRY;

  g_return_if_fail (GB_IS_SEARCH_DISPLAY (display));
  g_return_if_fail (GB_IS_SEARCH_PROVIDER (provider));

  for (iter = results; iter; iter = iter->next)
    {
      gtk_list_box_insert (display->priv->list_box, iter->data, -1);
    }

  //gtk_list_box_invalidate_sort (display->priv->list_box);

  EXIT;
}

GbSearchContext *
gb_search_display_get_context (GbSearchDisplay *display)
{
  g_return_val_if_fail (GB_IS_SEARCH_DISPLAY (display), NULL);

  return display->priv->context;
}

static void
gb_search_display_connect (GbSearchDisplay *display,
                           GbSearchContext *context)
{
  GbSearchDisplayPrivate *priv;
  const GList *list;
  const GList *iter;

  ENTRY;

  g_return_if_fail (GB_IS_SEARCH_DISPLAY (display));
  g_return_if_fail (GB_IS_SEARCH_CONTEXT (context));

  priv = display->priv;

  g_signal_connect_object (context,
                           "results-added",
                           G_CALLBACK (gb_search_display_results_added),
                           display,
                           G_CONNECT_SWAPPED);

  list = gb_search_context_get_results (context);
  for (iter = list; iter; iter = iter->next)
    gtk_list_box_insert (priv->list_box, iter->data, -1);
  //gtk_list_box_invalidate_sort (display->priv->list_box);

  EXIT;
}

static void
gb_search_display_disconnect (GbSearchDisplay *display,
                              GbSearchContext *context)
{
  GbSearchDisplayPrivate *priv;
  GList *children;
  GList *iter;

  ENTRY;

  g_return_if_fail (GB_IS_SEARCH_DISPLAY (display));
  g_return_if_fail (GB_IS_SEARCH_CONTEXT (context));

  priv = display->priv;

  g_signal_handlers_disconnect_by_func (context,
                                        G_CALLBACK (gb_search_display_results_added),
                                        display);

  children = gtk_container_get_children (GTK_CONTAINER (priv->list_box));
  for (iter = children; iter; iter = iter->next)
    gtk_container_remove (GTK_CONTAINER (priv->list_box), iter->data);
  g_list_free (children);

  EXIT;
}

void
gb_search_display_set_context (GbSearchDisplay *display,
                               GbSearchContext *context)
{
  ENTRY;

  g_return_if_fail (GB_IS_SEARCH_DISPLAY (display));
  g_return_if_fail (!context || GB_IS_SEARCH_CONTEXT (context));

  if (display->priv->context != context)
    {
      if (display->priv->context)
        {
          gb_search_display_disconnect (display, display->priv->context);
          g_clear_object (&display->priv->context);
        }

      if (context)
        {
          display->priv->context = g_object_ref (context);
          gb_search_display_connect (display, context);
        }

      g_object_notify_by_pspec (G_OBJECT (display),
                                gParamSpecs [PROP_CONTEXT]);
    }

  EXIT;
}

static void
gb_search_display_emit_result_activated (GbSearchDisplay *display,
                                         GbSearchResult  *result)
{
  g_return_if_fail (GB_IS_SEARCH_DISPLAY (display));
  g_return_if_fail (GB_IS_SEARCH_RESULT (result));

  gb_search_result_activate (result);
  g_signal_emit (display, gSignals [RESULT_ACTIVATED], 0, result);
}

static void
gb_search_display_row_activated (GbSearchDisplay *display,
                                 GtkListBoxRow   *row,
                                 GtkListBox      *list_box)
{
  GtkWidget *child;

  g_return_if_fail (GB_IS_SEARCH_DISPLAY (display));
  g_return_if_fail (GTK_IS_LIST_BOX_ROW (row));
  g_return_if_fail (GTK_IS_LIST_BOX (list_box));

  child = gtk_bin_get_child (GTK_BIN (row));

  if (GB_IS_SEARCH_RESULT (child))
    gb_search_display_emit_result_activated (display, GB_SEARCH_RESULT (child));
}

static void
gb_search_display_grab_focus (GtkWidget *widget)
{
  GbSearchDisplay *display = (GbSearchDisplay *)widget;
  GtkListBoxRow *row;

  g_return_if_fail (GB_IS_SEARCH_DISPLAY (display));

  /*
   * WORKAROUND:
   *
   * Getting the row at y of 0 does not work (returns NULL). And getting the
   * row at index 0 does not take into account sort.
   *
   * https://bugzilla.gnome.org/show_bug.cgi?id=741208
   */
  row = gtk_list_box_get_row_at_y (display->priv->list_box, 1);

  if (row)
    {
      gtk_list_box_select_row (display->priv->list_box, row);
      gtk_widget_grab_focus (GTK_WIDGET (row));
    }
}

static void
gb_search_display_constructed (GObject *object)
{
  GbSearchDisplay *self = (GbSearchDisplay *)object;

  g_return_if_fail (GB_IS_SEARCH_DISPLAY (self));

  G_OBJECT_CLASS (gb_search_display_parent_class)->constructed (object);

  g_signal_connect_object (self->priv->list_box,
                           "row-activated",
                           G_CALLBACK (gb_search_display_row_activated),
                           self,
                           G_CONNECT_SWAPPED);
}

static void
gb_search_display_finalize (GObject *object)
{
  GbSearchDisplayPrivate *priv = GB_SEARCH_DISPLAY (object)->priv;

  g_clear_object (&priv->context);

  G_OBJECT_CLASS (gb_search_display_parent_class)->finalize (object);
}

static void
gb_search_display_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  GbSearchDisplay *self = GB_SEARCH_DISPLAY (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      g_value_set_object (value, self->priv->context);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_search_display_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GbSearchDisplay *self = GB_SEARCH_DISPLAY (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      gb_search_display_set_context (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_search_display_class_init (GbSearchDisplayClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = gb_search_display_constructed;
  object_class->finalize = gb_search_display_finalize;
  object_class->get_property = gb_search_display_get_property;
  object_class->set_property = gb_search_display_set_property;

  widget_class->grab_focus = gb_search_display_grab_focus;

  gParamSpecs [PROP_CONTEXT] =
    g_param_spec_object ("context",
                         _("Context"),
                         _("The search context."),
                         GB_TYPE_SEARCH_CONTEXT,
                         (G_PARAM_READWRITE |
                          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_CONTEXT,
                                   gParamSpecs [PROP_CONTEXT]);

  gSignals [RESULT_ACTIVATED] =
    g_signal_new ("result-activated",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GbSearchDisplayClass, result_activated),
                  NULL,
                  NULL,
                  g_cclosure_marshal_generic,
                  G_TYPE_NONE,
                  1,
                  GB_TYPE_SEARCH_RESULT);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/builder/ui/gb-search-display.ui");
  gtk_widget_class_bind_template_child_private (widget_class, GbSearchDisplay, list_box);
}

static void
gb_search_display_init (GbSearchDisplay *self)
{
  self->priv = gb_search_display_get_instance_private (self);

  gtk_widget_init_template (GTK_WIDGET (self));
}
