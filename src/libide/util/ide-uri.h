/* GLIB - Library of useful routines for C programming
 * Copyright © 1999-2008 Novell, Inc.
 * Copyright © 2008-2010 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __IDE_URI_H__
#define __IDE_URI_H__

#include <gio/gio.h>

G_BEGIN_DECLS

typedef struct _IdeUri IdeUri;

typedef enum
{
  IDE_URI_PARSE_STRICT      = 1 << 0,
  IDE_URI_PARSE_HTML5       = 1 << 1,
  IDE_URI_PARSE_NO_IRI      = 1 << 2,
  IDE_URI_PARSE_PASSWORD    = 1 << 3,
  IDE_URI_PARSE_AUTH_PARAMS = 1 << 4,
  IDE_URI_PARSE_NON_DNS     = 1 << 5,
  IDE_URI_PARSE_DECODED     = 1 << 6,
  IDE_URI_PARSE_UTF8_ONLY   = 1 << 7
} IdeUriParseFlags;

IdeUri *       ide_uri_new           (const gchar        *uri_string,
                                      IdeUriParseFlags    flags,
                                      GError            **error);
IdeUri *       ide_uri_new_relative  (IdeUri             *base_uri,
                                      const gchar        *uri_string,
                                      IdeUriParseFlags    flags,
                                      GError            **error);
IdeUri *       ide_uri_new_from_file (GFile              *file);

typedef enum
{
  IDE_URI_HIDE_AUTH_PARAMS = 1 << 0,
  IDE_URI_HIDE_FRAGMENT    = 1 << 1,
} IdeUriToStringFlags;

#define IDE_TYPE_URI (ide_uri_get_type())

GType        ide_uri_get_type        (void);

char *       ide_uri_to_string       (IdeUri               *uri,
                                      IdeUriToStringFlags   flags);

IdeUri *     ide_uri_copy            (IdeUri               *uri);
IdeUri *     ide_uri_ref             (IdeUri               *uri);
void         ide_uri_unref           (IdeUri               *uri);

const gchar *ide_uri_get_scheme      (IdeUri               *uri);
void         ide_uri_set_scheme      (IdeUri               *uri,
                                      const gchar        *scheme);

const gchar *ide_uri_get_user        (IdeUri               *uri);
void         ide_uri_set_user        (IdeUri               *uri,
                                      const gchar        *user);

const gchar *ide_uri_get_password    (IdeUri               *uri);
void         ide_uri_set_password    (IdeUri               *uri,
                                      const gchar        *password);

const gchar *ide_uri_get_auth_params (IdeUri               *uri);
void         ide_uri_set_auth_params (IdeUri               *uri,
                                      const gchar        *auth_params);

const gchar *ide_uri_get_host        (IdeUri               *uri);
void         ide_uri_set_host        (IdeUri               *uri,
                                      const gchar        *host);

gushort      ide_uri_get_port        (IdeUri               *uri);
void         ide_uri_set_port        (IdeUri               *uri,
                                      gushort             port);

const gchar *ide_uri_get_path        (IdeUri               *uri);
void         ide_uri_set_path        (IdeUri               *uri,
                                      const gchar        *path);

const gchar *ide_uri_get_query       (IdeUri               *uri);
void         ide_uri_set_query       (IdeUri               *uri,
                                      const gchar        *query);

const gchar *ide_uri_get_fragment    (IdeUri               *uri);
void         ide_uri_set_fragment    (IdeUri               *uri,
                                      const gchar        *fragment);


void         ide_uri_split           (const gchar        *uri_string,
                                      gboolean            strict,
                                      gchar             **scheme,
                                      gchar             **userinfo,
                                      gchar             **host,
                                      gchar             **port,
                                      gchar             **path,
                                      gchar             **query,
                                      gchar             **fragment);
GHashTable * ide_uri_parse_params    (const gchar        *params,
                                      gssize              length,
                                      gchar               separator,
                                      gboolean            case_insensitive);
gboolean     ide_uri_parse_host      (const gchar        *uri_string,
                                      IdeUriParseFlags    flags,
                                      gchar             **scheme,
                                      gchar             **host,
                                      gushort            *port,
                                      GError            **error);

gchar *      ide_uri_build           (const gchar        *scheme,
                                      const gchar        *userinfo,
                                      const gchar        *host,
                                      const gchar        *port,
                                      const gchar        *path,
                                      const gchar        *query,
                                      const gchar        *fragment);

gboolean     ide_uri_is_file         (IdeUri             *uri,
                                      GFile              *file);

GFile       *ide_uri_to_file         (IdeUri             *uri);


/**
 * IDE_URI_ERROR:
 *
 * Error domain for URI methods. Errors in this domain will be from
 * the #IdeUriError enumeration. See #GError for information on error
 * domains.
 */
#define IDE_URI_ERROR (ide_uri_error_quark ())

/**
 * IdeUriError:
 * @IDE_URI_ERROR_PARSE: URI could not be parsed
 *
 * Error codes returned by #IdeUri methods.
 */
typedef enum
{
  IDE_URI_ERROR_MISC,
  IDE_URI_ERROR_BAD_SCHEME,
  IDE_URI_ERROR_BAD_USER,
  IDE_URI_ERROR_BAD_PASSWORD,
  IDE_URI_ERROR_BAD_AUTH_PARAMS,
  IDE_URI_ERROR_BAD_HOST,
  IDE_URI_ERROR_BAD_PORT,
  IDE_URI_ERROR_BAD_PATH,
  IDE_URI_ERROR_BAD_QUERY,
  IDE_URI_ERROR_BAD_FRAGMENT
} IdeUriError;

GQuark ide_uri_error_quark (void);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (IdeUri, ide_uri_unref)

G_END_DECLS

#endif /* __IDE_URI_H__ */
