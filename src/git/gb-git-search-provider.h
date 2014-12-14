/* gb-git-search-provider.h
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

#ifndef GB_GIT_SEARCH_PROVIDER_H
#define GB_GIT_SEARCH_PROVIDER_H

#include <glib-object.h>
#include <libgit2-glib/ggit.h>

#include "gb-search-provider.h"

G_BEGIN_DECLS

#define GB_TYPE_GIT_SEARCH_PROVIDER            (gb_git_search_provider_get_type())
#define GB_GIT_SEARCH_PROVIDER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_GIT_SEARCH_PROVIDER, GbGitSearchProvider))
#define GB_GIT_SEARCH_PROVIDER_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_GIT_SEARCH_PROVIDER, GbGitSearchProvider const))
#define GB_GIT_SEARCH_PROVIDER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GB_TYPE_GIT_SEARCH_PROVIDER, GbGitSearchProviderClass))
#define GB_IS_GIT_SEARCH_PROVIDER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GB_TYPE_GIT_SEARCH_PROVIDER))
#define GB_IS_GIT_SEARCH_PROVIDER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GB_TYPE_GIT_SEARCH_PROVIDER))
#define GB_GIT_SEARCH_PROVIDER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GB_TYPE_GIT_SEARCH_PROVIDER, GbGitSearchProviderClass))

typedef struct _GbGitSearchProvider        GbGitSearchProvider;
typedef struct _GbGitSearchProviderClass   GbGitSearchProviderClass;
typedef struct _GbGitSearchProviderPrivate GbGitSearchProviderPrivate;

struct _GbGitSearchProvider
{
  GObject parent;

  /*< private >*/
  GbGitSearchProviderPrivate *priv;
};

struct _GbGitSearchProviderClass
{
  GObjectClass parent;
};

GType             gb_git_search_provider_get_type       (void);
GbSearchProvider *gb_git_search_provider_new            (GgitRepository      *repository);
GgitRepository   *gb_git_search_provider_get_repository (GbGitSearchProvider *provider);
void              gb_git_search_provider_set_repository (GbGitSearchProvider *provider,
                                                         GgitRepository      *repository);

G_END_DECLS

#endif /* GB_GIT_SEARCH_PROVIDER_H */
