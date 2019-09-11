/* ide-clone-surface.c
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-clone-surface"

#include "config.h"

#include <glib/gi18n.h>
#include <libpeas/peas.h>
#include <libide-vcs.h>

#include "ide-clone-surface.h"
#include "ide-greeter-private.h"
#include "ide-greeter-workspace.h"

struct _IdeCloneSurface
{
  IdeSurface           parent_instance;

  /* This extension set contains IdeVcsCloner implementations which we
   * use to validate URIs, as well as provide some toggles for how the
   * user wants to perform the clone operation. Currently, we have a
   * very limited set of cloning (basically just git), but that could be
   * expanded in the future based on demand.
   */
  PeasExtensionSet    *addins;
  guint                n_addins;

  /* We calculate the file to the target folder based on the vcs uri and
   * the destination file chooser. It's cached here so that we don't have
   * to recaclulate it in multiple code paths.
   */
  GFile               *destination;

  /* Template Widgets */
  DzlFileChooserEntry *destination_chooser;
  GtkLabel            *destination_label;
  DzlRadioBox         *kind_radio;
  GtkLabel            *kind_label;
  GtkLabel            *status_message;
  GtkEntry            *uri_entry;
  GtkEntry            *author_entry;
  GtkEntry            *email_entry;
  GtkEntry            *branch_entry;
  GtkButton           *clone_button;
  GtkButton           *cancel_button;
  GtkStack            *button_stack;

  guint                dir_valid : 1;
  guint                vcs_valid : 1;
};

G_DEFINE_TYPE (IdeCloneSurface, ide_clone_surface, IDE_TYPE_SURFACE)

enum {
  PROP_0,
  PROP_URI,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

/**
 * ide_clone_surface_new:
 *
 * Create a new #IdeCloneSurface.
 *
 * Returns: (transfer full): a newly created #IdeCloneSurface
 *
 * Since: 3.32
 */
IdeCloneSurface *
ide_clone_surface_new (void)
{
  return g_object_new (IDE_TYPE_CLONE_SURFACE, NULL);
}

static void
ide_clone_surface_addin_added_cb (PeasExtensionSet *set,
                                  PeasPluginInfo   *plugin_info,
                                  PeasExtension    *exten,
                                  gpointer          user_data)
{
  IdeVcsCloner *cloner = (IdeVcsCloner *)exten;
  IdeCloneSurface *self = user_data;
  g_autofree gchar *title = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_VCS_CLONER (cloner));
  g_assert (IDE_IS_CLONE_SURFACE (self));

  self->n_addins++;

  title = ide_vcs_cloner_get_title (cloner);

  dzl_radio_box_add_item (self->kind_radio,
                          peas_plugin_info_get_module_name (plugin_info),
                          title);

  if (self->n_addins > 1)
    {
      gtk_widget_show (GTK_WIDGET (self->kind_label));
      gtk_widget_show (GTK_WIDGET (self->kind_radio));
    }
}

static void
ide_clone_surface_addin_removed_cb (PeasExtensionSet *set,
                                    PeasPluginInfo   *plugin_info,
                                    PeasExtension    *exten,
                                    gpointer          user_data)
{
  IdeCloneSurface *self = user_data;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_VCS_CLONER (exten));
  g_assert (IDE_IS_CLONE_SURFACE (self));

  self->n_addins--;

  dzl_radio_box_remove_item (self->kind_radio,
                             peas_plugin_info_get_module_name (plugin_info));

  if (self->n_addins < 2)
    {
      gtk_widget_hide (GTK_WIDGET (self->kind_label));
      gtk_widget_hide (GTK_WIDGET (self->kind_radio));
    }

  ide_object_destroy (IDE_OBJECT (exten));
}

static void
ide_clone_surface_validate_cb (PeasExtensionSet *set,
                               PeasPluginInfo   *plugin_info,
                               PeasExtension    *exten,
                               gpointer          user_data)
{
  IdeVcsCloner *cloner = (IdeVcsCloner *)exten;
  struct {
    const gchar *text;
    gchar       *errmsg;
    gboolean     valid;
  } *validate = user_data;
  g_autofree gchar *errmsg = NULL;

  g_assert (IDE_IS_VCS_CLONER (cloner));

  if (validate->valid)
    return;

  validate->valid = ide_vcs_cloner_validate_uri (cloner, validate->text, &errmsg);

  if (!validate->errmsg)
    validate->errmsg = g_steal_pointer (&errmsg);
}

static void
ide_clone_surface_validate (IdeCloneSurface *self)
{
  struct {
    const gchar *text;
    gchar       *errmsg;
    gboolean     valid;
  } validate;

  g_assert (IDE_IS_CLONE_SURFACE (self));

  validate.text = gtk_entry_get_text (self->uri_entry);
  validate.errmsg = NULL;
  validate.valid = FALSE;

  if (self->addins != NULL)
    peas_extension_set_foreach (self->addins,
                                ide_clone_surface_validate_cb,
                                &validate);

  if (validate.valid)
    dzl_gtk_widget_remove_style_class (GTK_WIDGET (self->uri_entry), "error");
  else
    dzl_gtk_widget_add_style_class (GTK_WIDGET (self->uri_entry), "error");

  if (validate.errmsg)
    gtk_widget_set_tooltip_text (GTK_WIDGET (self->uri_entry), validate.errmsg);
  else
    gtk_widget_set_tooltip_text (GTK_WIDGET (self->uri_entry), NULL);

  g_free (validate.errmsg);
}

static void
ide_clone_surface_update (IdeCloneSurface *self)
{
  g_autoptr(GFile) file = NULL;
  g_autoptr(GFile) child_file = NULL;
  g_autoptr(IdeVcsUri) uri = NULL;
  g_autofree gchar *child = NULL;
  g_autofree gchar *collapsed = NULL;
  g_autofree gchar *formatted = NULL;
  const gchar *text;
  GtkEntry *entry;

  g_assert (IDE_IS_CLONE_SURFACE (self));

  ide_clone_surface_validate (self);

  file = dzl_file_chooser_entry_get_file (self->destination_chooser);
  text = gtk_entry_get_text (self->uri_entry);
  uri = ide_vcs_uri_new (text);

  self->vcs_valid = uri != NULL;

  if (uri != NULL)
    child = ide_vcs_uri_get_clone_name (uri);

  if (child)
    child_file = g_file_get_child (file, child);
  else
    child_file = g_object_ref (file);

  g_set_object (&self->destination, child_file);

  collapsed = ide_path_collapse (g_file_peek_path (child_file));

  entry = dzl_file_chooser_entry_get_entry (self->destination_chooser);

  if (g_file_query_exists (child_file, NULL))
    {
      /* translators: %s is replaced with the path to the project */
      formatted = g_strdup_printf (_("The directory “%s” already exists. Please choose another directory."),
                                   collapsed);
      dzl_gtk_widget_add_style_class (GTK_WIDGET (entry), "error");
      self->dir_valid = FALSE;
    }
  else
    {
      /* translators: %s is replaced with the path to the project */
      formatted = g_strdup_printf (_("Your project will be created at %s"), collapsed);
      dzl_gtk_widget_remove_style_class (GTK_WIDGET (entry), "error");
      self->dir_valid = TRUE;
    }

  gtk_label_set_label (self->destination_label, formatted);

  gtk_widget_set_sensitive (GTK_WIDGET (self->clone_button),
                            self->dir_valid && self->vcs_valid);
}

static void
ide_clone_surface_uri_entry_changed (IdeCloneSurface *self,
                                     GtkEntry        *entry)
{
  g_assert (IDE_IS_CLONE_SURFACE (self));
  g_assert (GTK_IS_ENTRY (entry));

  ide_clone_surface_update (self);
}

static void
ide_clone_surface_destination_changed (IdeCloneSurface     *self,
                                       GParamSpec          *pspec,
                                       DzlFileChooserEntry *chooser)
{
  g_assert (IDE_IS_CLONE_SURFACE (self));
  g_assert (DZL_IS_FILE_CHOOSER_ENTRY (chooser));

  ide_clone_surface_update (self);
}

static void
ide_clone_surface_grab_focus (GtkWidget *widget)
{
  gtk_widget_grab_focus (GTK_WIDGET (IDE_CLONE_SURFACE (widget)->uri_entry));
}

static void
ide_clone_surface_destroy (GtkWidget *widget)
{
  IdeCloneSurface *self = (IdeCloneSurface *)widget;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_CLONE_SURFACE (self));

  g_clear_object (&self->addins);
  g_clear_object (&self->destination);

  GTK_WIDGET_CLASS (ide_clone_surface_parent_class)->destroy (widget);
}

static void
ide_clone_surface_context_set (GtkWidget  *widget,
                               IdeContext *context)
{
  IdeCloneSurface *self = (IdeCloneSurface *)widget;
  g_autoptr(GFile) file = NULL;

  g_assert (IDE_IS_CLONE_SURFACE (self));
  g_assert (!context || IDE_IS_CONTEXT (context));

  gtk_entry_set_text (self->author_entry, g_get_real_name ());

  file = g_file_new_for_path (ide_get_projects_dir ());
  dzl_file_chooser_entry_set_file (self->destination_chooser, file);

  if (context == NULL)
    return;

  self->addins = peas_extension_set_new (peas_engine_get_default (),
                                         IDE_TYPE_VCS_CLONER,
                                         "parent", context,
                                         NULL);

  g_signal_connect (self->addins,
                    "extension-added",
                    G_CALLBACK (ide_clone_surface_addin_added_cb),
                    self);

  g_signal_connect (self->addins,
                    "extension-removed",
                    G_CALLBACK (ide_clone_surface_addin_removed_cb),
                    self);

  peas_extension_set_foreach (self->addins,
                              ide_clone_surface_addin_added_cb,
                              self);

  ide_clone_surface_update (self);
}

static void
ide_clone_surface_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  IdeCloneSurface *self = IDE_CLONE_SURFACE (object);

  switch (prop_id)
    {
    case PROP_URI:
      g_value_set_string (value, ide_clone_surface_get_uri (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_clone_surface_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  IdeCloneSurface *self = IDE_CLONE_SURFACE (object);

  switch (prop_id)
    {
    case PROP_URI:
      ide_clone_surface_set_uri (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_clone_surface_class_init (IdeCloneSurfaceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = ide_clone_surface_get_property;
  object_class->set_property = ide_clone_surface_set_property;

  widget_class->destroy = ide_clone_surface_destroy;
  widget_class->grab_focus = ide_clone_surface_grab_focus;

  /**
   * IdeCloneSurface:uri:
   *
   * The "uri" property is the URI of the version control repository to
   * be cloned. Usually, this is something like
   *
   *   "https://gitlab.gnome.org/GNOME/gnome-builder.git"
   *
   * Since: 3.32
   */
  properties [PROP_URI] =
    g_param_spec_string ("uri",
                         "Uri",
                         "The URI of the repository to clone.",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/ui/ide-clone-surface.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeCloneSurface, author_entry);
  gtk_widget_class_bind_template_child (widget_class, IdeCloneSurface, branch_entry);
  gtk_widget_class_bind_template_child (widget_class, IdeCloneSurface, button_stack);
  gtk_widget_class_bind_template_child (widget_class, IdeCloneSurface, cancel_button);
  gtk_widget_class_bind_template_child (widget_class, IdeCloneSurface, clone_button);
  gtk_widget_class_bind_template_child (widget_class, IdeCloneSurface, destination_chooser);
  gtk_widget_class_bind_template_child (widget_class, IdeCloneSurface, destination_label);
  gtk_widget_class_bind_template_child (widget_class, IdeCloneSurface, email_entry);
  gtk_widget_class_bind_template_child (widget_class, IdeCloneSurface, kind_label);
  gtk_widget_class_bind_template_child (widget_class, IdeCloneSurface, kind_radio);
  gtk_widget_class_bind_template_child (widget_class, IdeCloneSurface, status_message);
  gtk_widget_class_bind_template_child (widget_class, IdeCloneSurface, uri_entry);
  gtk_widget_class_bind_template_callback (widget_class, ide_clone_surface_clone);
  gtk_widget_class_bind_template_callback (widget_class, ide_clone_surface_destination_changed);
  gtk_widget_class_bind_template_callback (widget_class, ide_clone_surface_uri_entry_changed);
}

static void
ide_clone_surface_init (IdeCloneSurface *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  ide_widget_set_context_handler (self, ide_clone_surface_context_set);
}

const gchar *
ide_clone_surface_get_uri (IdeCloneSurface *self)
{
  g_return_val_if_fail (IDE_IS_CLONE_SURFACE (self), NULL);

  return gtk_entry_get_text (self->uri_entry);
}

void
ide_clone_surface_set_uri (IdeCloneSurface *self,
                           const gchar     *uri)
{
  static const struct {
    const gchar *prefix;
    const gchar *expanded;
  } mappings[] = {
    { "gnome:",       "https://gitlab.gnome.org/" },
    { "freedesktop:", "https://gitlab.freedesktop.org/" },
    { "gitlab:",      "https://gitlab.com/" },
    { "github:",      "https://github.com/" },
  };
  g_autofree gchar *expanded = NULL;

  g_return_if_fail (IDE_IS_CLONE_SURFACE (self));

  if (uri != NULL)
    {
      for (guint i = 0; i < G_N_ELEMENTS (mappings); i++)
        {
          const gchar *prefix = mappings[i].prefix;

          if (g_str_has_prefix (uri, prefix))
            uri = expanded = g_strdup_printf ("%s%s", mappings[i].expanded, uri + strlen (prefix));
        }
    }

  gtk_entry_set_text (self->uri_entry, uri);
}

static void
ide_clone_surface_clone_cb (GObject      *object,
                            GAsyncResult *result,
                            gpointer      user_data)
{
  IdeVcsCloner *cloner = (IdeVcsCloner *)object;
  g_autoptr(IdeCloneSurface) self = user_data;
  g_autoptr(IdeProjectInfo) project_info = NULL;
  g_autoptr(GError) error = NULL;
  GtkWidget *workspace;

  g_assert (IDE_IS_VCS_CLONER (cloner));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_CLONE_SURFACE (self));

  workspace = gtk_widget_get_ancestor (GTK_WIDGET (self), IDE_TYPE_GREETER_WORKSPACE);
  ide_greeter_workspace_end (IDE_GREETER_WORKSPACE (workspace));

  gtk_widget_set_sensitive (GTK_WIDGET (self->uri_entry), TRUE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->destination_chooser), TRUE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->clone_button), TRUE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->author_entry), TRUE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->email_entry), TRUE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->branch_entry), TRUE);
  gtk_stack_set_visible_child (self->button_stack, GTK_WIDGET (self->clone_button));
  gtk_label_set_label (self->status_message, "");

  if (!ide_vcs_cloner_clone_finish (cloner, result, &error))
    {
      g_warning ("Failed to clone repository: %s", error->message);
      gtk_label_set_label (self->status_message, error->message);
      gtk_entry_set_progress_fraction (self->uri_entry, 0);
      return;
    }

  project_info = ide_project_info_new ();
  ide_project_info_set_vcs_uri (project_info, gtk_entry_get_text (self->uri_entry));
  ide_project_info_set_file (project_info, self->destination);
  ide_project_info_set_directory (project_info, self->destination);

  ide_greeter_workspace_open_project (IDE_GREETER_WORKSPACE (workspace), project_info);
}

void
ide_clone_surface_clone (IdeCloneSurface *self)
{
  PeasEngine *engine = peas_engine_get_default ();
  g_autoptr(IdeNotification) notif = NULL;
  g_autoptr(GCancellable) cancellable = NULL;
  PeasPluginInfo *plugin_info;
  IdeVcsCloner *addin;
  GVariantDict dict;
  const gchar *branch;
  const gchar *uri;
  const gchar *path;
  const gchar *module_name;
  const gchar *author;
  const gchar *email;
  GtkWidget *workspace;

  g_return_if_fail (IDE_IS_CLONE_SURFACE (self));

  if (!(module_name = dzl_radio_box_get_active_id (self->kind_radio)) ||
      !(plugin_info = peas_engine_get_plugin_info (engine, module_name)) ||
      !(addin = (IdeVcsCloner *)peas_extension_set_get_extension (self->addins, plugin_info)))
    {
      g_warning ("Failed to locate module to use for cloning");
      return;
    }

  g_variant_dict_init (&dict, NULL);

  uri = gtk_entry_get_text (self->uri_entry);
  author = gtk_entry_get_text (self->author_entry);
  email = gtk_entry_get_text (self->email_entry);
  path = g_file_peek_path (self->destination);
  branch = gtk_entry_get_text (self->branch_entry);

  if (!ide_str_empty0 (branch))
    g_variant_dict_insert (&dict, "branch", "s", branch);

  if (!ide_str_empty0 (author) && !g_str_equal (g_get_real_name (), author))
    g_variant_dict_insert (&dict, "user.name", "s", author);

  if (!ide_str_empty0 (email))
    g_variant_dict_insert (&dict, "user.email", "s", email);

  g_debug ("Cloning repository using addin: %s", module_name);

  workspace = gtk_widget_get_ancestor (GTK_WIDGET (self), IDE_TYPE_GREETER_WORKSPACE);
  ide_greeter_workspace_begin (IDE_GREETER_WORKSPACE (workspace));

  cancellable = g_cancellable_new ();

  g_signal_connect_object (self->cancel_button,
                           "clicked",
                           G_CALLBACK (g_cancellable_cancel),
                           cancellable,
                           G_CONNECT_SWAPPED);

  notif = ide_notification_new ();

  ide_vcs_cloner_clone_async (addin,
                              uri,
                              path,
                              g_variant_dict_end (&dict),
                              notif,
                              cancellable,
                              ide_clone_surface_clone_cb,
                              g_object_ref (self));

  gtk_widget_set_sensitive (GTK_WIDGET (self->uri_entry), FALSE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->destination_chooser), FALSE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->clone_button), FALSE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->author_entry), FALSE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->email_entry), FALSE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->branch_entry), FALSE);
  gtk_stack_set_visible_child (self->button_stack, GTK_WIDGET (self->cancel_button));

  g_object_bind_property (notif, "progress", self->uri_entry, "progress-fraction", G_BINDING_SYNC_CREATE);
  g_object_bind_property (notif, "body", self->status_message, "label", G_BINDING_SYNC_CREATE);
}
