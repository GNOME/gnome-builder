/* gbp-gobject-template.c
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-gobject-template"

#include "gbp-gobject-template.h"

struct _GbpGobjectTemplate
{
  IdeTemplateBase parent_instance;

  GbpGobjectSpec     *spec;
  GFile              *directory;
  GbpGobjectLanguage  language;
};

enum {
  PROP_0,
  PROP_DIRECTORY,
  PROP_SPEC,
  N_PROPS
};

G_DEFINE_TYPE (GbpGobjectTemplate, gbp_gobject_template, IDE_TYPE_TEMPLATE_BASE)

static GParamSpec *properties [N_PROPS];

static void
gbp_gobject_template_finalize (GObject *object)
{
  GbpGobjectTemplate *self = (GbpGobjectTemplate *)object;

  g_clear_object (&self->directory);
  g_clear_object (&self->spec);

  G_OBJECT_CLASS (gbp_gobject_template_parent_class)->finalize (object);
}

static void
gbp_gobject_template_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  GbpGobjectTemplate *self = GBP_GOBJECT_TEMPLATE (object);

  switch (prop_id)
    {
    case PROP_DIRECTORY:
      g_value_set_object (value, self->directory);
      break;

    case PROP_SPEC:
      g_value_set_object (value, self->spec);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_gobject_template_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  GbpGobjectTemplate *self = GBP_GOBJECT_TEMPLATE (object);

  switch (prop_id)
    {
    case PROP_DIRECTORY:
      gbp_gobject_template_set_directory (self, g_value_get_object (value));
      break;

    case PROP_SPEC:
      gbp_gobject_template_set_spec (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_gobject_template_class_init (GbpGobjectTemplateClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gbp_gobject_template_finalize;
  object_class->get_property = gbp_gobject_template_get_property;
  object_class->set_property = gbp_gobject_template_set_property;

  properties [PROP_DIRECTORY] =
    g_param_spec_object ("directory",
                         NULL,
                         NULL,
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_SPEC] =
    g_param_spec_object ("spec",
                         NULL,
                         NULL,
                         GBP_TYPE_GOBJECT_SPEC,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_gobject_template_init (GbpGobjectTemplate *self)
{
}

void
gbp_gobject_template_set_spec (GbpGobjectTemplate *self,
                               GbpGobjectSpec     *spec)
{
  g_return_if_fail (GBP_IS_GOBJECT_TEMPLATE (self));
  g_return_if_fail (GBP_IS_GOBJECT_SPEC (spec));

  if (g_set_object (&self->spec, spec))
    g_object_notify_by_pspec (G_OBJECT (spec), properties [PROP_SPEC]);
}

void
gbp_gobject_template_set_directory (GbpGobjectTemplate *self,
                                    GFile              *directory)
{
  g_return_if_fail (GBP_IS_GOBJECT_TEMPLATE (self));
  g_return_if_fail (G_IS_FILE (directory));

  if (g_set_object (&self->directory, directory))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_DIRECTORY]);
}

static gchar *
mangle_name (const gchar *name)
{
  GString *symbol_name = g_string_new ("");
  gint i;

  /* copied from gtkbuilder.c */

  for (i = 0; name[i] != '\0'; i++)
    {
      /* skip if uppercase, first or previous is uppercase */
      if ((name[i] == g_ascii_toupper (name[i]) &&
           i > 0 && name[i-1] != g_ascii_toupper (name[i-1])) ||
          (i > 2 && name[i]   == g_ascii_toupper (name[i]) &&
           name[i-1] == g_ascii_toupper (name[i-1]) &&
           name[i-2] == g_ascii_toupper (name[i-2])))
        g_string_append_c (symbol_name, '_');
      g_string_append_c (symbol_name, g_ascii_tolower (name[i]));
    }

  return g_string_free (symbol_name, FALSE);
}

static gchar *
mangle_parent_type (const gchar *name)
{
  g_autofree gchar *mangled = NULL;
  g_auto(GStrv) parts = NULL;
  GString *str;

  if (!name || !*name)
    return g_strdup ("G_TYPE_OBJECT");

  str = g_string_new (NULL);
  mangled = mangle_name (name);
  parts = g_strsplit (mangled, "_", 0);

  if (NULL == strchr (mangled, '_'))
    {
      /* Hrmm, looks like we might have a G* parent */
      if (*mangled == 'g')
        {
          g_autofree gchar *upper = g_utf8_strup (name + 1, -1);

          return g_strdup_printf ("G_TYPE_%s", upper);
        }
    }

  for (guint i = 0; parts[i]; i++)
    {
      g_autofree gchar *upper = g_utf8_strup (parts[i], -1);

      if (i != 0)
        g_string_append_c (str, '_');
      g_string_append (str, upper);
      if (i == 0)
        g_string_append (str, "_TYPE");
    }

  return g_string_free (str, FALSE);
}

static void
gbp_gobject_template_add_c_resources (GbpGobjectTemplate *self)
{
  g_autofree gchar *mangled = NULL;
  g_autofree gchar *mangled_dash = NULL;
  g_autofree gchar *mangled_upper = NULL;
  g_autofree gchar *c_name = NULL;
  g_autofree gchar *h_name = NULL;
  g_autofree gchar *namespace_upper = NULL;
  g_autofree gchar *class_name_upper = NULL;
  g_autofree gchar *namespace_lower = NULL;
  g_autofree gchar *class_name_lower = NULL;
  g_autofree gchar *class_name_mangled = NULL;
  g_autofree gchar *namespace_mangled = NULL;
  g_autofree gchar *parent_mangled_upper = NULL;
  g_autoptr(GFile) c_dest = NULL;
  g_autoptr(GFile) h_dest = NULL;
  g_autoptr(TmplScope) scope = NULL;
  const gchar *class_name;
  const gchar *namespace;
  const gchar *name;
  const gchar *parent_name;

  g_assert (GBP_IS_GOBJECT_TEMPLATE (self));
  g_assert (GBP_IS_GOBJECT_SPEC (self->spec));
  g_assert (G_IS_FILE (self->directory));
  g_assert (self->language == GBP_GOBJECT_LANGUAGE_C);

  name = gbp_gobject_spec_get_name (self->spec);
  class_name = gbp_gobject_spec_get_class_name (self->spec);
  namespace = gbp_gobject_spec_get_namespace (self->spec);
  parent_name = gbp_gobject_spec_get_parent_name (self->spec);

  mangled = mangle_name (name);
  mangled_dash = g_strdelimit (g_strdup (mangled), "_", '-');
  mangled_upper = g_utf8_strup (mangled, -1);

  class_name_mangled = mangle_name (class_name);
  namespace_mangled = mangle_name (namespace);

  namespace_upper = g_utf8_strup (namespace_mangled, -1);
  class_name_upper = g_utf8_strup (class_name_mangled, -1);

  namespace_lower = g_utf8_strdown (mangled, -1);
  class_name_lower = g_utf8_strdown (mangled, -1);

  c_name = g_strdup_printf ("%s.c", mangled_dash);
  h_name = g_strdup_printf ("%s.h", mangled_dash);

  c_dest = g_file_get_child (self->directory, c_name);
  h_dest = g_file_get_child (self->directory, h_name);

  parent_mangled_upper = mangle_parent_type (parent_name);

  scope = tmpl_scope_new ();

  tmpl_symbol_assign_object (tmpl_scope_get (scope, "spec"), self->spec);

  tmpl_symbol_assign_string (tmpl_scope_get (scope, "file_prefix"), mangled_dash);

  tmpl_symbol_assign_string (tmpl_scope_get (scope, "Class"), class_name);
  tmpl_symbol_assign_string (tmpl_scope_get (scope, "Name"), name);
  tmpl_symbol_assign_string (tmpl_scope_get (scope, "Namespace"), namespace);

  tmpl_symbol_assign_string (tmpl_scope_get (scope, "CLASS"), class_name_upper);
  tmpl_symbol_assign_string (tmpl_scope_get (scope, "NAMESPACE"), namespace_upper);
  tmpl_symbol_assign_string (tmpl_scope_get (scope, "NAME"), mangled_upper);

  tmpl_symbol_assign_string (tmpl_scope_get (scope, "PARENT_TYPE"), parent_mangled_upper);

#define ADD_SPACE(name, n) \
  G_STMT_START { \
    GString *space = g_string_new (NULL); \
    guint count = (n); \
    for (guint i = 0; i < count; i++) \
      g_string_append_c (space, ' '); \
    tmpl_symbol_assign_string (tmpl_scope_get (scope, name), space->str); \
    g_string_free (space, TRUE); \
  } G_STMT_END

  ADD_SPACE ("space", strlen (mangled));
  ADD_SPACE ("Space", strlen (name));

  tmpl_symbol_assign_string (tmpl_scope_get (scope, "class"), class_name_lower);
  tmpl_symbol_assign_string (tmpl_scope_get (scope, "namespace"), namespace_lower);
  tmpl_symbol_assign_string (tmpl_scope_get (scope, "name"), mangled);

  tmpl_symbol_assign_string (tmpl_scope_get (scope, "Parent"), parent_name);

  ide_template_base_add_resource (IDE_TEMPLATE_BASE (self),
                                  "/org/gnome/builder/plugins/gobject-templates/gobject.c.tmpl",
                                  c_dest,
                                  scope,
                                  0640);

  ide_template_base_add_resource (IDE_TEMPLATE_BASE (self),
                                  "/org/gnome/builder/plugins/gobject-templates/gobject.h.tmpl",
                                  h_dest,
                                  scope,
                                  0640);
}

void
gbp_gobject_template_set_language (GbpGobjectTemplate *self,
                                   GbpGobjectLanguage  language)
{
  g_return_if_fail (GBP_IS_GOBJECT_TEMPLATE (self));

  self->language = language;

  switch (language)
    {
    case GBP_GOBJECT_LANGUAGE_C:
      gbp_gobject_template_add_c_resources (self);
      break;

    case GBP_GOBJECT_LANGUAGE_CPLUSPLUS:
      break;

    case GBP_GOBJECT_LANGUAGE_PYTHON:
      break;

    case GBP_GOBJECT_LANGUAGE_VALA:
      break;

    default:
      g_assert_not_reached ();
    }
}

GbpGobjectTemplate *
gbp_gobject_template_new (void)
{
  return g_object_new (GBP_TYPE_GOBJECT_TEMPLATE, NULL);
}
