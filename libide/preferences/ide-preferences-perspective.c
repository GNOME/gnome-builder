/* ide-preferences-perspective.c
 *
 * Copyright (C) 2015 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-preferences-perspective"

#include <glib/gi18n.h>
#include <libpeas/peas.h>

#include "ide-perspective.h"
#include "ide-preferences.h"
#include "ide-preferences-addin.h"
#include "ide-preferences-builtin.h"
#include "ide-preferences-font-button.h"
#include "ide-preferences-group.h"
#include "ide-preferences-page.h"
#include "ide-preferences-perspective.h"
#include "ide-preferences-spin-button.h"
#include "ide-preferences-switch.h"
#include "ide-workbench-header-bar.h"

struct _IdePreferencesPerspective
{
  GtkBin                 parent_instance;

  guint                  last_widget_id;

  PeasExtensionSet      *extensions;
  GSequence             *pages;
  GHashTable            *widgets;

  GtkStack              *page_stack;
  GtkStackSwitcher      *page_stack_sidebar;
  IdeWorkbenchHeaderBar *titlebar;
};

static void ide_preferences_iface_init (IdePreferencesInterface *iface);
static void ide_perspective_iface_init (IdePerspectiveInterface *iface);

G_DEFINE_TYPE_EXTENDED (IdePreferencesPerspective, ide_preferences_perspective, GTK_TYPE_BIN, 0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_PREFERENCES, ide_preferences_iface_init)
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_PERSPECTIVE, ide_perspective_iface_init))

enum {
  PROP_0,
  LAST_PROP
};

static GParamSpec *properties [LAST_PROP];

static gint
sort_by_priority (gconstpointer a,
                  gconstpointer b,
                  gpointer      user_data)
{
  gint prioritya = 0;
  gint priorityb = 0;

  g_object_get ((gpointer)a, "priority", &prioritya, NULL);
  g_object_get ((gpointer)b, "priority", &priorityb, NULL);

  return prioritya - priorityb;
}

static void
ide_preferences_perspective_extension_added (PeasExtensionSet *set,
                                             PeasPluginInfo   *plugin_info,
                                             PeasExtension    *extension,
                                             gpointer          user_data)
{
  IdePreferencesPerspective *self = user_data;

  ide_preferences_addin_load (IDE_PREFERENCES_ADDIN (extension), IDE_PREFERENCES (self));
}

static void
ide_preferences_perspective_extension_removed (PeasExtensionSet *set,
                                               PeasPluginInfo   *plugin_info,
                                               PeasExtension    *extension,
                                               gpointer          user_data)
{
  IdePreferencesPerspective *self = user_data;

  ide_preferences_addin_unload (IDE_PREFERENCES_ADDIN (extension), IDE_PREFERENCES (self));
}

static void
ide_preferences_perspective_constructed (GObject *object)
{
  IdePreferencesPerspective *self = (IdePreferencesPerspective *)object;

  G_OBJECT_CLASS (ide_preferences_perspective_parent_class)->constructed (object);

  _ide_preferences_builtin_register (IDE_PREFERENCES (self));

  self->extensions = peas_extension_set_new (peas_engine_get_default (),
                                             IDE_TYPE_PREFERENCES_ADDIN,
                                             NULL);

  g_signal_connect (self->extensions,
                    "extension-added",
                    G_CALLBACK (ide_preferences_perspective_extension_added),
                    self);

  g_signal_connect (self->extensions,
                    "extension-removed",
                    G_CALLBACK (ide_preferences_perspective_extension_removed),
                    self);

  peas_extension_set_foreach (self->extensions,
                              ide_preferences_perspective_extension_added,
                              self);
}

static void
ide_preferences_perspective_finalize (GObject *object)
{
  IdePreferencesPerspective *self = (IdePreferencesPerspective *)object;

  g_clear_pointer (&self->pages, g_sequence_free);
  g_clear_pointer (&self->widgets, g_hash_table_unref);

  G_OBJECT_CLASS (ide_preferences_perspective_parent_class)->finalize (object);
}

static void
ide_preferences_perspective_get_property (GObject    *object,
                                          guint       prop_id,
                                          GValue     *value,
                                          GParamSpec *pspec)
{
  //IdePreferencesPerspective *self = IDE_PREFERENCES_PERSPECTIVE (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_preferences_perspective_set_property (GObject      *object,
                                          guint         prop_id,
                                          const GValue *value,
                                          GParamSpec   *pspec)
{
  //IdePreferencesPerspective *self = IDE_PREFERENCES_PERSPECTIVE (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_preferences_perspective_class_init (IdePreferencesPerspectiveClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = ide_preferences_perspective_constructed;
  object_class->finalize = ide_preferences_perspective_finalize;
  object_class->get_property = ide_preferences_perspective_get_property;
  object_class->set_property = ide_preferences_perspective_set_property;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/ui/ide-preferences-perspective.ui");
  gtk_widget_class_bind_template_child (widget_class, IdePreferencesPerspective, page_stack_sidebar);
  gtk_widget_class_bind_template_child (widget_class, IdePreferencesPerspective, page_stack);
  gtk_widget_class_bind_template_child (widget_class, IdePreferencesPerspective, titlebar);
}

static void
ide_preferences_perspective_init (IdePreferencesPerspective *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->pages = g_sequence_new (NULL);
  self->widgets = g_hash_table_new (g_direct_hash, g_direct_equal);
}

static void
ide_preferences_perspective_add_page (IdePreferences *preferences,
                                      const gchar    *page_name,
                                      const gchar    *title,
                                      gint            priority)
{
  IdePreferencesPerspective *self = (IdePreferencesPerspective *)preferences;
  IdePreferencesPage *page;
  GSequenceIter *iter;

  g_assert (IDE_IS_PREFERENCES (preferences));
  g_assert (IDE_IS_PREFERENCES_PERSPECTIVE (self));
  g_assert (page_name != NULL);
  g_assert (title != NULL);

  if (gtk_stack_get_child_by_name (self->page_stack, page_name))
    {
      g_warning ("A preference page named %s has already been registered.",
                 page_name);
      return;
    }

  page = g_object_new (IDE_TYPE_PREFERENCES_PAGE,
                       "priority", priority,
                       "visible", TRUE,
                       NULL);
  iter = g_sequence_insert_sorted (self->pages, page, sort_by_priority, NULL);
  gtk_container_add_with_properties (GTK_CONTAINER (self->page_stack), GTK_WIDGET (page),
                                     "position", g_sequence_iter_get_position (iter),
                                     "name", page_name,
                                     "title", title,
                                     NULL);
}

static void
ide_preferences_perspective_add_group (IdePreferences *preferences,
                                       const gchar    *page_name,
                                       const gchar    *group_name,
                                       const gchar    *title,
                                       gint            priority)
{
  IdePreferencesPerspective *self = (IdePreferencesPerspective *)preferences;
  IdePreferencesGroup *group;
  GtkWidget *page;

  g_assert (IDE_IS_PREFERENCES_PERSPECTIVE (self));
  g_assert (page_name != NULL);
  g_assert (group_name != NULL);

  page = gtk_stack_get_child_by_name (self->page_stack, page_name);

  if (page == NULL)
    {
      g_warning ("No page named \"%s\" could be found.", page_name);
      return;
    }

  group = g_object_new (IDE_TYPE_PREFERENCES_GROUP,
                        "name", group_name,
                        "priority", priority,
                        "title", title,
                        "visible", TRUE,
                        NULL);
  ide_preferences_page_add_group (IDE_PREFERENCES_PAGE (page), group);
}

static void
ide_preferences_perspective_add_list_group  (IdePreferences *preferences,
                                             const gchar    *page_name,
                                             const gchar    *group_name,
                                             const gchar    *title,
                                             gint            priority)
{
  IdePreferencesPerspective *self = (IdePreferencesPerspective *)preferences;
  IdePreferencesGroup *group;
  GtkWidget *page;

  g_assert (IDE_IS_PREFERENCES_PERSPECTIVE (self));
  g_assert (page_name != NULL);
  g_assert (group_name != NULL);

  page = gtk_stack_get_child_by_name (self->page_stack, page_name);

  if (page == NULL)
    {
      g_warning ("No page named \"%s\" could be found.", page_name);
      return;
    }

  group = g_object_new (IDE_TYPE_PREFERENCES_GROUP,
                        "is-list", TRUE,
                        "name", group_name,
                        "priority", priority,
                        "title", title,
                        "visible", TRUE,
                        NULL);
  ide_preferences_page_add_group (IDE_PREFERENCES_PAGE (page), group);
}

static guint
ide_preferences_perspective_add_radio (IdePreferences *preferences,
                                       const gchar    *page_name,
                                       const gchar    *group_name,
                                       const gchar    *schema_id,
                                       const gchar    *key,
                                       const gchar    *path,
                                       const gchar    *variant_string,
                                       const gchar    *title,
                                       const gchar    *subtitle,
                                       const gchar    *keywords,
                                       gint            priority)
{
  IdePreferencesPerspective *self = (IdePreferencesPerspective *)preferences;
  IdePreferencesSwitch *widget;
  IdePreferencesGroup *group;
  g_autoptr(GVariant) variant = NULL;
  GtkWidget *page;
  guint widget_id;

  g_assert (IDE_IS_PREFERENCES_PERSPECTIVE (self));
  g_assert (page_name != NULL);
  g_assert (group_name != NULL);
  g_assert (schema_id != NULL);
  g_assert (key != NULL);
  g_assert (title != NULL);

  page = gtk_stack_get_child_by_name (self->page_stack, page_name);

  if (page == NULL)
    {
      g_warning ("No page named \"%s\" could be found.", page_name);
      return 0;
    }

  group = ide_preferences_page_get_group (IDE_PREFERENCES_PAGE (page), group_name);

  if (group == NULL)
    {
      g_warning ("No such preferences group \"%s\" in page \"%s\"",
                 group_name, page_name);
      return 0;
    }

  if (variant_string != NULL)
    {
      g_autoptr(GError) error = NULL;

      variant = g_variant_parse (NULL, variant_string, NULL, NULL, &error);

      if (variant == NULL)
        g_warning ("%s", error->message);
      else
        g_variant_ref_sink (variant);
    }

  widget = g_object_new (IDE_TYPE_PREFERENCES_SWITCH,
                         "is-radio", TRUE,
                         "key", key,
                         "keywords", keywords,
                         "path", path,
                         "priority", priority,
                         "schema-id", schema_id,
                         "subtitle", subtitle,
                         "target", variant,
                         "title", title,
                         "visible", TRUE,
                         NULL);

  ide_preferences_group_add (group, GTK_WIDGET (widget));

  widget_id = ++self->last_widget_id;
  g_hash_table_insert (self->widgets, GINT_TO_POINTER (widget_id), widget);

  return widget_id;
}

static guint
ide_preferences_perspective_add_switch (IdePreferences *preferences,
                                        const gchar    *page_name,
                                        const gchar    *group_name,
                                        const gchar    *schema_id,
                                        const gchar    *key,
                                        const gchar    *path,
                                        const gchar    *variant_string,
                                        const gchar    *title,
                                        const gchar    *subtitle,
                                        const gchar    *keywords,
                                        gint            priority)
{
  IdePreferencesPerspective *self = (IdePreferencesPerspective *)preferences;
  IdePreferencesSwitch *widget;
  IdePreferencesGroup *group;
  g_autoptr(GVariant) variant = NULL;
  GtkWidget *page;
  guint widget_id;

  g_assert (IDE_IS_PREFERENCES_PERSPECTIVE (self));
  g_assert (page_name != NULL);
  g_assert (group_name != NULL);
  g_assert (schema_id != NULL);
  g_assert (key != NULL);
  g_assert (title != NULL);

  page = gtk_stack_get_child_by_name (self->page_stack, page_name);

  if (page == NULL)
    {
      g_warning ("No page named \"%s\" could be found.", page_name);
      return 0;
    }

  group = ide_preferences_page_get_group (IDE_PREFERENCES_PAGE (page), group_name);

  if (group == NULL)
    {
      g_warning ("No such preferences group \"%s\" in page \"%s\"",
                 group_name, page_name);
      return 0;
    }

  if (variant_string != NULL)
    {
      g_autoptr(GError) error = NULL;

      variant = g_variant_parse (NULL, variant_string, NULL, NULL, &error);

      if (variant == NULL)
        g_warning ("%s", error->message);
      else
        g_variant_ref_sink (variant);
    }

  widget = g_object_new (IDE_TYPE_PREFERENCES_SWITCH,
                         "key", key,
                         "keywords", keywords,
                         "path", path,
                         "priority", priority,
                         "schema-id", schema_id,
                         "subtitle", subtitle,
                         "target", variant,
                         "title", title,
                         "visible", TRUE,
                         NULL);

  ide_preferences_group_add (group, GTK_WIDGET (widget));

  widget_id = ++self->last_widget_id;
  g_hash_table_insert (self->widgets, GINT_TO_POINTER (widget_id), widget);

  return widget_id;
}

static guint
ide_preferences_perspective_add_spin_button (IdePreferences *preferences,
                                             const gchar    *page_name,
                                             const gchar    *group_name,
                                             const gchar    *schema_id,
                                             const gchar    *key,
                                             const gchar    *path,
                                             const gchar    *title,
                                             const gchar    *subtitle,
                                             const gchar    *keywords,
                                             gint            priority)
{
  IdePreferencesPerspective *self = (IdePreferencesPerspective *)preferences;
  IdePreferencesSpinButton *widget;
  IdePreferencesGroup *group;
  GtkWidget *page;
  guint widget_id;

  g_assert (IDE_IS_PREFERENCES_PERSPECTIVE (self));
  g_assert (page_name != NULL);
  g_assert (group_name != NULL);
  g_assert (schema_id != NULL);
  g_assert (key != NULL);
  g_assert (title != NULL);

  page = gtk_stack_get_child_by_name (self->page_stack, page_name);

  if (page == NULL)
    {
      g_warning ("No page named \"%s\" could be found.", page_name);
      return 0;
    }

  group = ide_preferences_page_get_group (IDE_PREFERENCES_PAGE (page), group_name);


  if (group == NULL)
    {
      g_warning ("No such preferences group \"%s\" in page \"%s\"",
                 group_name, page_name);
      return 0;
    }

  widget = g_object_new (IDE_TYPE_PREFERENCES_SPIN_BUTTON,
                         "key", key,
                         "keywords", keywords,
                         "path", path,
                         "priority", priority,
                         "schema-id", schema_id,
                         "subtitle", subtitle,
                         "title", title,
                         "visible", TRUE,
                         NULL);

  ide_preferences_group_add (group, GTK_WIDGET (widget));

  widget_id = ++self->last_widget_id;
  g_hash_table_insert (self->widgets, GINT_TO_POINTER (widget_id), widget);

  return widget_id;
}

static guint
ide_preferences_perspective_add_font_button (IdePreferences *preferences,
                                             const gchar    *page_name,
                                             const gchar    *group_name,
                                             const gchar    *schema_id,
                                             const gchar    *key,
                                             const gchar    *title,
                                             const gchar    *keywords,
                                             gint            priority)
{
  IdePreferencesPerspective *self = (IdePreferencesPerspective *)preferences;
  IdePreferencesSwitch *widget;
  IdePreferencesGroup *group;
  GtkWidget *page;
  guint widget_id;

  g_assert (IDE_IS_PREFERENCES_PERSPECTIVE (self));
  g_assert (page_name != NULL);
  g_assert (group_name != NULL);
  g_assert (schema_id != NULL);
  g_assert (key != NULL);
  g_assert (title != NULL);

  page = gtk_stack_get_child_by_name (self->page_stack, page_name);

  if (page == NULL)
    {
      g_warning ("No page named \"%s\" could be found.", page_name);
      return 0;
    }

  group = ide_preferences_page_get_group (IDE_PREFERENCES_PAGE (page), group_name);

  if (group == NULL)
    {
      g_warning ("No such preferences group \"%s\" in page \"%s\"",
                 group_name, page_name);
      return 0;
    }

  widget = g_object_new (IDE_TYPE_PREFERENCES_FONT_BUTTON,
                         "key", key,
                         "keywords", keywords,
                         "priority", priority,
                         "schema-id", schema_id,
                         "title", title,
                         "visible", TRUE,
                         NULL);

  ide_preferences_group_add (group, GTK_WIDGET (widget));

  widget_id = ++self->last_widget_id;
  g_hash_table_insert (self->widgets, GINT_TO_POINTER (widget_id), widget);

  return widget_id;
}

static guint
ide_preferences_perspective_add_custom (IdePreferences *preferences,
                                        const gchar    *page_name,
                                        const gchar    *group_name,
                                        GtkWidget      *widget,
                                        const gchar    *keywords,
                                        gint            priority)
{
  IdePreferencesPerspective *self = (IdePreferencesPerspective *)preferences;
  IdePreferencesBin *container;
  IdePreferencesGroup *group;
  GtkWidget *page;
  guint widget_id;

  g_assert (IDE_IS_PREFERENCES_PERSPECTIVE (self));
  g_assert (page_name != NULL);
  g_assert (group_name != NULL);
  g_assert (GTK_IS_WIDGET (widget));

  page = gtk_stack_get_child_by_name (self->page_stack, page_name);

  if (page == NULL)
    {
      g_warning ("No page named \"%s\" could be found.", page_name);
      return 0;
    }

  group = ide_preferences_page_get_group (IDE_PREFERENCES_PAGE (page), group_name);

  if (group == NULL)
    {
      g_warning ("No such preferences group \"%s\" in page \"%s\"",
                 group_name, page_name);
      return 0;
    }

  container = g_object_new (IDE_TYPE_PREFERENCES_BIN,
                            "child", widget,
                            "keywords", keywords,
                            "priority", priority,
                            "visible", TRUE,
                            NULL);

  ide_preferences_group_add (group, GTK_WIDGET (container));

  widget_id = ++self->last_widget_id;
  g_hash_table_insert (self->widgets, GINT_TO_POINTER (widget_id), widget);

  return widget_id;
}

static void
ide_preferences_iface_init (IdePreferencesInterface *iface)
{
  iface->add_page = ide_preferences_perspective_add_page;
  iface->add_group = ide_preferences_perspective_add_group;
  iface->add_list_group  = ide_preferences_perspective_add_list_group ;
  iface->add_radio = ide_preferences_perspective_add_radio;
  iface->add_font_button = ide_preferences_perspective_add_font_button;
  iface->add_switch = ide_preferences_perspective_add_switch;
  iface->add_spin_button = ide_preferences_perspective_add_spin_button;
  iface->add_custom = ide_preferences_perspective_add_custom;
}

static gchar *
ide_preferences_perspective_get_title (IdePerspective *perspective)
{
  return g_strdup (_("Preferences"));
}

static gchar *
ide_preferences_perspective_get_icon_name (IdePerspective *perspective)
{
  return g_strdup ("preferences-system-symbolic");
}

static GtkWidget *
ide_preferences_perspective_get_titlebar (IdePerspective *perspective)
{
  return GTK_WIDGET (IDE_PREFERENCES_PERSPECTIVE (perspective)->titlebar);
}

static void
ide_perspective_iface_init (IdePerspectiveInterface *iface)
{
  iface->get_icon_name = ide_preferences_perspective_get_icon_name;
  iface->get_title = ide_preferences_perspective_get_title;
  iface->get_titlebar = ide_preferences_perspective_get_titlebar;
}
