/* gbp-platformui-tweaks-addin.c
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

#define G_LOG_DOMAIN "gbp-platformui-tweaks-addin"

#include "config.h"

#include <glib/gi18n.h>

#include <libide-gui.h>

#include "gbp-platformui-tweaks-addin.h"

#include "ide-style-variant-preview-private.h"

struct _GbpPlatformuiTweaksAddin
{
  IdeTweaksAddin parent_instance;
};

G_DEFINE_FINAL_TYPE (GbpPlatformuiTweaksAddin, gbp_platformui_tweaks_addin, IDE_TYPE_TWEAKS_ADDIN)

static const struct {
  const char *key;
  AdwColorScheme color_scheme;
  const char *title;
} variants[] = {
  { "default", ADW_COLOR_SCHEME_DEFAULT, N_("Follow System") },
  { "light", ADW_COLOR_SCHEME_FORCE_LIGHT, N_("Light") },
  { "dark", ADW_COLOR_SCHEME_FORCE_DARK, N_("Dark") },
};

static GtkWidget *
platformui_create_style_selector (IdeTweaks       *tweaks,
                                  IdeTweaksWidget *widget,
                                  IdeTweaksWidget *instance)
{
  GtkBox *box;

  g_assert (IDE_IS_TWEAKS_WIDGET (widget));
  g_assert (IDE_IS_TWEAKS (tweaks));

  box = g_object_new (GTK_TYPE_BOX,
                      "css-name", "list",
                      "homogeneous", TRUE,
                      NULL);
  gtk_widget_add_css_class (GTK_WIDGET (box), "boxed-list");
  gtk_widget_add_css_class (GTK_WIDGET (box), "style-variant");

  for (guint i = 0; i < G_N_ELEMENTS (variants); i++)
    {
      IdeStyleVariantPreview *preview;
      GtkInscription *label;
      GtkButton *button;
      GtkBox *vbox;

      vbox = g_object_new (GTK_TYPE_BOX,
                           "orientation", GTK_ORIENTATION_VERTICAL,
                           "spacing", 8,
                           "margin-top", 18,
                           "margin-bottom", 18,
                           "margin-start", 9,
                           "margin-end", 9,
                           NULL);
      preview = g_object_new (IDE_TYPE_STYLE_VARIANT_PREVIEW,
                              "color-scheme", variants[i].color_scheme,
                              NULL);
      button = g_object_new (GTK_TYPE_TOGGLE_BUTTON,
                             "action-name", "app.style-variant",
                             "child", preview,
                             NULL);
      gtk_actionable_set_action_target (GTK_ACTIONABLE (button), "s", variants[i].key);
      label = g_object_new (GTK_TYPE_INSCRIPTION,
                            "xalign", .5f,
                            "text", g_dgettext (NULL, variants[i].title),
                            "tooltip-text", g_dgettext (NULL, variants[i].title),
                            "text-overflow", GTK_INSCRIPTION_OVERFLOW_ELLIPSIZE_END,
                            NULL);
      gtk_box_append (vbox, GTK_WIDGET (button));
      gtk_box_append (vbox, GTK_WIDGET (label));
      gtk_box_append (box, GTK_WIDGET (vbox));
    }

  return GTK_WIDGET (box);
}

static void
gbp_platformui_tweaks_addin_class_init (GbpPlatformuiTweaksAddinClass *klass)
{
}

static void
gbp_platformui_tweaks_addin_init (GbpPlatformuiTweaksAddin *self)
{
  ide_tweaks_addin_set_resource_paths (IDE_TWEAKS_ADDIN (self),
                                       IDE_STRV_INIT ("/plugins/platformui/tweaks.ui"));
  ide_tweaks_addin_bind_callback (IDE_TWEAKS_ADDIN (self),
                                  platformui_create_style_selector);
}
