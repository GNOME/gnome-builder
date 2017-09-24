{{include "license.h"}}

#pragma once

#include <glib.h>

G_BEGIN_DECLS

#define {{NAME}}_INSIDE
# include "{{prefix}}-version.h"
#undef {{NAME}}_INSIDE

G_END_DECLS
