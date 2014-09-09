/* gb-tab-label.c
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

#define G_LOG_DOMAIN "tab-label"

#include <glib/gi18n.h>

#include "gb-log.h"
#include "gb-tab.h"
#include "gb-tab-label.h"
#include "gedit-close-button.h"

struct _GbTabLabelPrivate
{
  GbTab          *tab;

  GBinding       *title_binding;

  GtkWidget      *hbox;
  GtkWidget      *label;
  GtkWidget      *close_button;

  guint           button_pressed : 1;
};

enum {
  PROP_0,
  PROP_TAB,
  LAST_PROP
};

enum {
  CLOSE_CLICKED,
  LAST_SIGNAL
};

G_DEFINE_TYPE_WITH_PRIVATE (GbTabLabel, gb_tab_label, GTK_TYPE_BIN)

static GParamSpec *gParamSpecs[LAST_PROP];
static guint       gSignals[LAST_SIGNAL];

GtkWidget *
gb_tab_label_new (GbTab *tab)
{
  return g_object_new (GB_TYPE_TAB_LABEL, "tab", tab, NULL);
}

void
_gb_tab_label_set_show_close_button (GbTabLabel *tab_label,
                                     gboolean    show_close_button)
{
  g_return_if_fail (GB_IS_TAB_LABEL (tab_label));

  gtk_widget_set_visible (tab_label->priv->close_button, show_close_button);
}

GbTab *
gb_tab_label_get_tab (GbTabLabel *label)
{
  g_return_val_if_fail (GB_IS_TAB_LABEL (label), NULL);

  return label->priv->tab;
}

static void
gb_tab_label_set_tab (GbTabLabel *label,
                      GbTab      *tab)
{
  GbTabLabelPrivate *priv;

  g_return_if_fail (GB_IS_TAB_LABEL (label));
  g_return_if_fail (!tab || GB_IS_TAB (tab));

  priv = label->priv;

  g_clear_object (&priv->title_binding);

  if (priv->tab)
    {
      g_object_remove_weak_pointer (G_OBJECT (priv->tab),
                                    (gpointer *) &priv->tab);
      priv->tab = NULL;
    }

  if (tab)
    {
      priv->tab = tab;
      g_object_add_weak_pointer (G_OBJECT (tab), (gpointer *) &priv->tab);

      priv->title_binding =
        g_object_bind_property (tab, "title", priv->label, "label",
                                G_BINDING_SYNC_CREATE);
      g_object_add_weak_pointer (G_OBJECT (priv->title_binding),
                                 (gpointer *) &priv->title_binding);
    }
}

static void
on_close_button_clicked (GbTabLabel       *tab_label,
                         GeditCloseButton *close_button)
{
  ENTRY;

  g_return_if_fail (GB_IS_TAB_LABEL (tab_label));
  g_return_if_fail (GEDIT_IS_CLOSE_BUTTON (close_button));

  g_signal_emit (tab_label, gSignals [CLOSE_CLICKED], 0);

  EXIT;
}

static void
gb_tab_label_constructed (GObject *object)
{
  GbTabLabel *tab_label = (GbTabLabel *)object;

  g_return_if_fail (GB_IS_TAB_LABEL (tab_label));

  g_signal_connect_object (tab_label->priv->close_button,
                           "clicked",
                           G_CALLBACK (on_close_button_clicked),
                           tab_label,
                           G_CONNECT_SWAPPED);
}

static void
gb_tab_label_finalize (GObject *object)
{
  GbTabLabelPrivate *priv = GB_TAB_LABEL (object)->priv;

  g_clear_object (&priv->title_binding);

  if (priv->tab)
    {
      g_object_remove_weak_pointer (G_OBJECT (priv->tab),
                                    (gpointer *) &priv->tab);
      priv->tab = NULL;
    }

  G_OBJECT_CLASS (gb_tab_label_parent_class)->finalize (object);
}

static void
gb_tab_label_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  GbTabLabel *label = GB_TAB_LABEL (object);

  switch (prop_id)
    {
    case PROP_TAB:
      g_value_set_object (value, gb_tab_label_get_tab (label));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_tab_label_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  GbTabLabel *label = GB_TAB_LABEL (object);

  switch (prop_id)
    {
    case PROP_TAB:
      gb_tab_label_set_tab (label, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_tab_label_class_init (GbTabLabelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = gb_tab_label_constructed;
  object_class->finalize = gb_tab_label_finalize;
  object_class->get_property = gb_tab_label_get_property;
  object_class->set_property = gb_tab_label_set_property;

  gParamSpecs[PROP_TAB] =
    g_param_spec_object ("tab",
                         _ ("Tab"),
                         _ ("The tab the label is observing."),
                         GB_TYPE_TAB,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_TAB,
                                   gParamSpecs[PROP_TAB]);

  gSignals[CLOSE_CLICKED] =
    g_signal_new ("close-clicked",
                  GB_TYPE_TAB_LABEL,
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GbTabLabelClass, close_clicked),
                  NULL,
                  NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE,
                  0);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/builder/ui/gb-tab-label.ui");
  gtk_widget_class_bind_template_child_private (widget_class, GbTabLabel, hbox);
  gtk_widget_class_bind_template_child_private (widget_class, GbTabLabel, label);
  gtk_widget_class_bind_template_child_private (widget_class, GbTabLabel, close_button);

  g_type_ensure (GEDIT_TYPE_CLOSE_BUTTON);
}

static void
gb_tab_label_init (GbTabLabel *label)
{
  label->priv = gb_tab_label_get_instance_private (label);

  gtk_widget_init_template (GTK_WIDGET (label));
}
