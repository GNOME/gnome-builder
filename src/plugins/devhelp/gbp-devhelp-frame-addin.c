/* gbp-devhelp-frame-addin.c
 *
 * Copyright 2017-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-devhelp-frame-addin"

#include "gbp-devhelp-frame-addin.h"
#include "gbp-devhelp-menu-button.h"
#include "gbp-devhelp-page.h"

struct _GbpDevhelpFrameAddin
{
  GObject               parent_instance;
  IdeFrame       *stack;
  GbpDevhelpMenuButton *button;
};

static void
gbp_devhelp_frame_addin_search (GSimpleAction *action,
                                       GVariant      *variant,
                                       gpointer       user_data)
{
  GbpDevhelpFrameAddin *self = user_data;
  const gchar *keyword;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (GBP_IS_DEVHELP_FRAME_ADDIN (self));
  g_assert (self->stack != NULL);
  g_assert (IDE_IS_FRAME (self->stack));
  g_assert (variant != NULL);
  g_assert (g_variant_is_of_type (variant, G_VARIANT_TYPE_STRING));

  if (self->button != NULL)
    {
      keyword = g_variant_get_string (variant, NULL);
      gbp_devhelp_menu_button_search (self->button, keyword);
    }
}

static void
gbp_devhelp_frame_addin_new_view (GSimpleAction *action,
                                         GVariant      *variant,
                                         gpointer       user_data)
{
  GbpDevhelpFrameAddin *self = user_data;
  GbpDevhelpPage *view;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (GBP_IS_DEVHELP_FRAME_ADDIN (self));
  g_assert (self->stack != NULL);
  g_assert (IDE_IS_FRAME (self->stack));

  view = g_object_new (GBP_TYPE_DEVHELP_PAGE,
                       "visible", TRUE,
                       NULL);
  gtk_container_add (GTK_CONTAINER (self->stack), GTK_WIDGET (view));
}

static void
gbp_devhelp_frame_addin_navigate_to (GSimpleAction *action,
                                            GVariant      *variant,
                                            gpointer       user_data)
{
  GbpDevhelpFrameAddin *self = user_data;
  IdePage *view;
  const gchar *uri;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (GBP_IS_DEVHELP_FRAME_ADDIN (self));
  g_assert (self->stack != NULL);
  g_assert (IDE_IS_FRAME (self->stack));
  g_assert (variant != NULL);
  g_assert (g_variant_is_of_type (variant, G_VARIANT_TYPE_STRING));

  uri = g_variant_get_string (variant, NULL);
  view = ide_frame_get_visible_child (self->stack);

  if (GBP_IS_DEVHELP_PAGE (view))
    gbp_devhelp_page_set_uri (GBP_DEVHELP_PAGE (view), uri);
}

static GActionEntry actions[] = {
  { "new-view", gbp_devhelp_frame_addin_new_view },
  { "search", gbp_devhelp_frame_addin_search, "s" },
  { "navigate-to", gbp_devhelp_frame_addin_navigate_to, "s" },
};

static void
gbp_devhelp_frame_addin_load (IdeFrameAddin *addin,
                                     IdeFrame      *stack)
{
  GbpDevhelpFrameAddin *self = (GbpDevhelpFrameAddin *)addin;
  g_autoptr(GSimpleActionGroup) group = NULL;

  g_assert (GBP_IS_DEVHELP_FRAME_ADDIN (self));
  g_assert (IDE_IS_FRAME (stack));

  self->stack = stack;

  group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (group),
                                   actions,
                                   G_N_ELEMENTS (actions),
                                   self);
  gtk_widget_insert_action_group (GTK_WIDGET (stack),
                                  "devhelp",
                                  G_ACTION_GROUP (group));
}

static void
gbp_devhelp_frame_addin_unload (IdeFrameAddin *addin,
                                       IdeFrame      *stack)
{
  GbpDevhelpFrameAddin *self = (GbpDevhelpFrameAddin *)addin;

  g_assert (GBP_IS_DEVHELP_FRAME_ADDIN (self));
  g_assert (IDE_IS_FRAME (stack));

  self->stack = NULL;

  gtk_widget_insert_action_group (GTK_WIDGET (stack), "devhelp", NULL);

  if (self->button != NULL)
    gtk_widget_destroy (GTK_WIDGET (self->button));
}

static void
gbp_devhelp_frame_addin_set_view (IdeFrameAddin *addin,
                                         IdePage       *view)
{
  GbpDevhelpFrameAddin *self = (GbpDevhelpFrameAddin *)addin;
  gboolean visible = FALSE;

  g_assert (GBP_IS_DEVHELP_FRAME_ADDIN (self));
  g_assert (!view || IDE_IS_PAGE (view));
  g_assert (self->stack != NULL);
  g_assert (IDE_IS_FRAME (self->stack));

  /*
   * We don't setup self->button until we get our first devhelp
   * view. This helps reduce startup overhead as well as lower
   * memory footprint until it is necessary.
   */

  if (GBP_IS_DEVHELP_PAGE (view))
    {
      if (self->button == NULL)
        {
          GtkWidget *titlebar;

          titlebar = ide_frame_get_titlebar (self->stack);

          self->button = g_object_new (GBP_TYPE_DEVHELP_MENU_BUTTON,
                                       "hexpand", TRUE,
                                       NULL);
          g_signal_connect (self->button,
                            "destroy",
                            G_CALLBACK (gtk_widget_destroyed),
                            &self->button);
          ide_frame_header_add_custom_title (IDE_FRAME_HEADER (titlebar),
                                                    GTK_WIDGET (self->button),
                                                    100);
        }

      visible = TRUE;
    }

  if (self->button != NULL)
    gtk_widget_set_visible (GTK_WIDGET (self->button), visible);
}

static void
frame_addin_iface_init (IdeFrameAddinInterface *iface)
{
  iface->load = gbp_devhelp_frame_addin_load;
  iface->unload = gbp_devhelp_frame_addin_unload;
  iface->set_page = gbp_devhelp_frame_addin_set_view;
}

G_DEFINE_TYPE_WITH_CODE (GbpDevhelpFrameAddin,
                         gbp_devhelp_frame_addin,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_FRAME_ADDIN,
                                                frame_addin_iface_init))

static void
gbp_devhelp_frame_addin_class_init (GbpDevhelpFrameAddinClass *klass)
{
}

static void
gbp_devhelp_frame_addin_init (GbpDevhelpFrameAddin *self)
{
}
