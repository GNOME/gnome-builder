/* Make sure generated code works with older glib versions without g_memdup2 */
static inline gpointer
_memdup2(gconstpointer mem, gsize byte_size)
{
#if GLIB_CHECK_VERSION(2, 68, 0)
    return g_memdup2(mem, byte_size);
#else
    gpointer new_mem;

    if (mem && byte_size != 0) {
        new_mem = g_malloc(byte_size);
        memcpy(new_mem, mem, byte_size);
    } else {
        new_mem = NULL;
    }

    return new_mem;
#endif
}

#define REF_READ_FRAME_OFFSET(_v, _index) ref_read_unaligned_le ((guchar*)((_v).base) + (_v).size - (offset_size * ((_index) + 1)), offset_size)
#define REF_ALIGN(_offset, _align_to) ((_offset + _align_to - 1) & ~(gsize)(_align_to - 1))

/* Note: clz is undefinded for 0, so never call this size == 0 */
G_GNUC_CONST static inline guint
ref_get_offset_size (gsize size)
{
#if defined(__GNUC__) && (__GNUC__ >= 4) && defined(__OPTIMIZE__) && defined(__LP64__)
  /* Instead of using a lookup table we use nibbles in a lookup word */
  guint32 v = (guint32)0x88884421;
  return (v >> (((__builtin_clzl(size) ^ 63) / 8) * 4)) & 0xf;
#else
  if (size > G_MAXUINT16)
    {
      if (size > G_MAXUINT32)
        return 8;
      else
        return 4;
    }
  else
    {
      if (size > G_MAXUINT8)
         return 2;
      else
         return 1;
    }
#endif
}

G_GNUC_PURE static inline guint64
ref_read_unaligned_le (guchar *bytes, guint   size)
{
  union
  {
    guchar bytes[8];
    guint64 integer;
  } tmpvalue;

  tmpvalue.integer = 0;
  /* we unroll the size checks here so that memcpy gets constant args */
  if (size >= 4)
    {
      if (size == 8)
        memcpy (&tmpvalue.bytes, bytes, 8);
      else
        memcpy (&tmpvalue.bytes, bytes, 4);
    }
  else
    {
      if (size == 2)
        memcpy (&tmpvalue.bytes, bytes, 2);
      else
        memcpy (&tmpvalue.bytes, bytes, 1);
    }

  return GUINT64_FROM_LE (tmpvalue.integer);
}

static inline void
__gstring_append_double (GString *string, double d)
{
  gchar buffer[100];
  gint i;

  g_ascii_dtostr (buffer, sizeof buffer, d);
  for (i = 0; buffer[i]; i++)
    if (buffer[i] == '.' || buffer[i] == 'e' ||
        buffer[i] == 'n' || buffer[i] == 'N')
      break;

  /* if there is no '.' or 'e' in the float then add one */
  if (buffer[i] == '\0')
    {
      buffer[i++] = '.';
      buffer[i++] = '0';
      buffer[i++] = '\0';
    }
   g_string_append (string, buffer);
}

static inline void
__gstring_append_string (GString *string, const char *str)
{
  gunichar quote = strchr (str, '\'') ? '"' : '\'';

  g_string_append_c (string, quote);
  while (*str)
    {
      gunichar c = g_utf8_get_char (str);

      if (c == quote || c == '\\')
        g_string_append_c (string, '\\');

      if (g_unichar_isprint (c))
        g_string_append_unichar (string, c);
      else
        {
          g_string_append_c (string, '\\');
          if (c < 0x10000)
            switch (c)
              {
              case '\a':
                g_string_append_c (string, 'a');
                break;

              case '\b':
                g_string_append_c (string, 'b');
                break;

              case '\f':
                g_string_append_c (string, 'f');
                break;

              case '\n':
                g_string_append_c (string, 'n');
                break;

              case '\r':
                g_string_append_c (string, 'r');
                break;

              case '\t':
                g_string_append_c (string, 't');
                break;

              case '\v':
                g_string_append_c (string, 'v');
                break;

              default:
                g_string_append_printf (string, "u%04x", c);
                break;
              }
           else
             g_string_append_printf (string, "U%08x", c);
        }

      str = g_utf8_next_char (str);
    }

  g_string_append_c (string, quote);
}

/************** VariantRef *******************/

static inline Ref
variant_get_child (VariantRef v, const GVariantType **out_type)
{
  if (v.size)
    {
      guchar *base = (guchar *)v.base;
      gsize size = v.size - 1;

      /* find '\0' character */
      while (size > 0 && base[size] != 0)
        size--;

      /* ensure we didn't just hit the start of the string */
      if (base[size] == 0)
       {
          const char *type_string = (char *) base + size + 1;
          const char *limit = (char *)base + v.size;
          const char *end;

          if (g_variant_type_string_scan (type_string, limit, &end) && end == limit)
            {
              if (out_type)
                *out_type = (const GVariantType *)type_string;
              return (Ref) { v.base, size };
            }
       }
    }
  if (out_type)
    *out_type = G_VARIANT_TYPE_UNIT;
  return  (Ref) { "\0", 1 };
}

static inline const GVariantType *
variant_get_type (VariantRef v)
{
  if (v.size)
    {
      guchar *base = (guchar *)v.base;
      gsize size = v.size - 1;

      /* find '\0' character */
      while (size > 0 && base[size] != 0)
        size--;

      /* ensure we didn't just hit the start of the string */
      if (base[size] == 0)
       {
          const char *type_string = (char *) base + size + 1;
          const char *limit = (char *)base + v.size;
          const char *end;

          if (g_variant_type_string_scan (type_string, limit, &end) && end == limit)
             return (const GVariantType *)type_string;
       }
    }
  return  G_VARIANT_TYPE_UNIT;
}

static inline gboolean
variant_is_type (VariantRef v, const GVariantType *type)
{
   return g_variant_type_equal (variant_get_type (v), type);
}

static inline VariantRef
variant_from_gvariant (GVariant *v)
{
  g_assert (g_variant_type_equal (g_variant_get_type (v), G_VARIANT_TYPE_VARIANT));
  return (VariantRef) { g_variant_get_data (v), g_variant_get_size (v) };
}

static inline VariantRef
variant_from_bytes (GBytes *b)
{
  return (VariantRef) { g_bytes_get_data (b, NULL), g_bytes_get_size (b) };
}

static inline VariantRef
variant_from_data (gconstpointer data, gsize size)
{
  return (VariantRef) { data, size };
}

static inline GVariant *
variant_dup_to_gvariant (VariantRef v)
{
  guint8 *duped = _memdup2 (v.base, v.size);
  return g_variant_new_from_data (G_VARIANT_TYPE_VARIANT, duped, v.size, TRUE, g_free, duped);
}

static inline GVariant *
variant_to_gvariant (VariantRef v,
                              GDestroyNotify      notify,
                              gpointer            user_data)
{
  return g_variant_new_from_data (G_VARIANT_TYPE_VARIANT, v.base, v.size, TRUE, notify, user_data);
}

static inline GVariant *
variant_to_owned_gvariant (VariantRef v,
                                     GVariant *base)
{
  return variant_to_gvariant (v, (GDestroyNotify)g_variant_unref, g_variant_ref (base));
}

static inline GVariant *
variant_peek_as_variant (VariantRef v)
{
  return g_variant_new_from_data (G_VARIANT_TYPE_VARIANT, v.base, v.size, TRUE, NULL, NULL);
}

static inline VariantRef
variant_from_variant (VariantRef v)
{
  const GVariantType  *type;
  Ref child = variant_get_child (v, &type);
  g_assert (g_variant_type_equal(type, G_VARIANT_TYPE_VARIANT));
  return variant_from_data (child.base, child.size);
}

static inline GVariant *
variant_dup_child_to_gvariant (VariantRef v)
{
  const GVariantType  *type;
  Ref child = variant_get_child (v, &type);
  guint8 *duped = _memdup2 (child.base, child.size);
  return g_variant_new_from_data (type, duped, child.size, TRUE, g_free, duped);
}

static inline GVariant *
variant_peek_child_as_variant (VariantRef v)
{
  const GVariantType  *type;
  Ref child = variant_get_child (v, &type);
  return g_variant_new_from_data (type, child.base, child.size, TRUE, NULL, NULL);
}

static inline GString *
variant_format (VariantRef v, GString *s, gboolean type_annotate)
{
#ifdef DEEP_VARIANT_FORMAT
  GVariant *gv = variant_peek_as_variant (v);
  return g_variant_print_string (gv, s, TRUE);
#else
  const GVariantType  *type = variant_get_type (v);
  g_string_append_printf (s, "<@%.*s>", (int)g_variant_type_get_string_length (type), (const char *)type);
  return s;
#endif
}

static inline char *
variant_print (VariantRef v, gboolean type_annotate)
{
  GString *s = g_string_new ("");
  variant_format (v, s, type_annotate);
  return g_string_free (s, FALSE);
}
static inline gboolean
variant_get_boolean (VariantRef v)
{
  return (gboolean)*((guint8 *)v.base);
}
static inline guint8
variant_get_byte (VariantRef v)
{
  return (guint8)*((guint8 *)v.base);
}
static inline gint16
variant_get_int16 (VariantRef v)
{
  return (gint16)*((gint16 *)v.base);
}
static inline guint16
variant_get_uint16 (VariantRef v)
{
  return (guint16)*((guint16 *)v.base);
}
static inline gint32
variant_get_int32 (VariantRef v)
{
  return (gint32)*((gint32 *)v.base);
}
static inline guint32
variant_get_uint32 (VariantRef v)
{
  return (guint32)*((guint32 *)v.base);
}
static inline gint64
variant_get_int64 (VariantRef v)
{
  return (gint64)*((gint64 *)v.base);
}
static inline guint64
variant_get_uint64 (VariantRef v)
{
  return (guint64)*((guint64 *)v.base);
}
static inline guint32
variant_get_handle (VariantRef v)
{
  return (guint32)*((guint32 *)v.base);
}
static inline double
variant_get_double (VariantRef v)
{
  return (double)*((double *)v.base);
}
static inline const char *
variant_get_string (VariantRef v)
{
  return (const char *)v.base;
}
static inline const char *
variant_get_objectpath (VariantRef v)
{
  return (const char *)v.base;
}
static inline const char *
variant_get_signature (VariantRef v)
{
  return (const char *)v.base;
}

/************** Proposal *******************/

static inline ProposalRef
proposal_from_gvariant (GVariant *v)
{
  g_assert (g_variant_type_equal (g_variant_get_type (v), PROPOSAL_TYPESTRING));
  return (ProposalRef) { g_variant_get_data (v), g_variant_get_size (v) };
}

static inline ProposalRef
proposal_from_bytes (GBytes *b)
{
  return (ProposalRef) { g_bytes_get_data (b, NULL), g_bytes_get_size (b) };
}

static inline ProposalRef
proposal_from_data (gconstpointer data, gsize size)
{
  return (ProposalRef) { data, size };
}

static inline GVariant *
proposal_dup_to_gvariant (ProposalRef v)
{
  guint8 *duped = _memdup2 (v.base, v.size);
  return g_variant_new_from_data (PROPOSAL_TYPEFORMAT, duped, v.size, TRUE, g_free, duped);
}

static inline GVariant *
proposal_to_gvariant (ProposalRef v,
                             GDestroyNotify      notify,
                             gpointer            user_data)
{
  return g_variant_new_from_data (PROPOSAL_TYPEFORMAT, v.base, v.size, TRUE, notify, user_data);
}

static inline GVariant *
proposal_to_owned_gvariant (ProposalRef v, GVariant *base)
{
  return proposal_to_gvariant (v, (GDestroyNotify)g_variant_unref, g_variant_ref (base));
}

static inline GVariant *
proposal_peek_as_gvariant (ProposalRef v)
{
  return g_variant_new_from_data (PROPOSAL_TYPEFORMAT, v.base, v.size, TRUE, NULL, NULL);
}

static inline ProposalRef
proposal_from_variant (VariantRef v)
{
  const GVariantType  *type;
  Ref child = variant_get_child (v, &type);
  g_assert (g_variant_type_equal(type, PROPOSAL_TYPESTRING));
  return proposal_from_data (child.base, child.size);
}


static inline gsize
proposal_get_length (ProposalRef v)
{
  if (v.size == 0)
    return 0;
  guint offset_size = ref_get_offset_size (v.size);
  gsize last_end = REF_READ_FRAME_OFFSET(v, 0);
  gsize offsets_array_size;
  if (last_end > v.size)
    return 0;
  offsets_array_size = v.size - last_end;
  if (offsets_array_size % offset_size != 0)
    return 0;
  gsize length = offsets_array_size / offset_size;
  return length;
}

static inline ProposalEntryRef
proposal_get_at (ProposalRef v, gsize index)
{
  ProposalEntryRef res;
  guint offset_size = ref_get_offset_size (v.size);
  gsize last_end = REF_READ_FRAME_OFFSET(v, 0);
  gsize len = (v.size - last_end) / offset_size;
  gsize start = (index > 0) ? REF_ALIGN(REF_READ_FRAME_OFFSET(v, len - index), 8) : 0;
  gsize end = REF_READ_FRAME_OFFSET(v, len - index - 1);
  g_assert (start <= end);
  g_assert (end <= last_end);
  res = (ProposalEntryRef) { ((const char *)v.base) + start, end - start };
  return res;
}

static inline const char *
proposal_entry_get_key (ProposalEntryRef v)
{
  guint offset_size = ref_get_offset_size (v.size);
  G_GNUC_UNUSED gsize end = REF_READ_FRAME_OFFSET(v, 0);
  const char *base = (const char *)v.base;
  g_assert (end < v.size);
  g_assert (base[end-1] == 0);
  return base;
}

static inline VariantRef
proposal_entry_get_value (ProposalEntryRef v)
{
  guint offset_size = ref_get_offset_size (v.size);
  gsize end = REF_READ_FRAME_OFFSET(v, 0);
  gsize offset = REF_ALIGN(end, 8);
  g_assert (offset <= v.size);
  return (VariantRef) { (char *)v.base + offset, (v.size - offset_size) - offset };
}

static inline gboolean
proposal_lookup (ProposalRef v, const char * key, gsize *index_out, VariantRef *out)
{
  const char * canonical_key = key;
  if (v.size == 0)
    return FALSE;
  guint offset_size = ref_get_offset_size (v.size);
  gsize last_end = REF_READ_FRAME_OFFSET(v, 0);
  if (last_end > v.size)
    return FALSE;
  gsize offsets_array_size = v.size - last_end;
  if (offsets_array_size % offset_size != 0)
    return FALSE;
  gsize len = offsets_array_size / offset_size;
  gsize start = 0;
  gsize i;

  for (i = 0; i < len; i++)
    {
      gsize end = REF_READ_FRAME_OFFSET(v, len - i - 1);
      ProposalEntryRef e = { ((const guchar *)v.base) + start, end - start };
      g_assert (start <= end);
      g_assert (end <= last_end);
      const char * e_key = proposal_entry_get_key (e);
      if (strcmp(canonical_key, e_key) == 0)
        {
           if (index_out)
             *index_out = i;
           if (out)
             *out = proposal_entry_get_value (e);
           return TRUE;
        }
      start = REF_ALIGN(end, 8);
    }
    return FALSE;
}

static inline gboolean
proposal_lookup_boolean (ProposalRef v, const char * key, gboolean default_value)
{
   VariantRef value_v;

  if (proposal_lookup (v, key, NULL, &value_v) &&
      *(const char *)variant_get_type (value_v) == 'b')
    return variant_get_boolean (value_v);
  return default_value;
}

static inline guint8
proposal_lookup_byte (ProposalRef v, const char * key, guint8 default_value)
{
   VariantRef value_v;

  if (proposal_lookup (v, key, NULL, &value_v) &&
      *(const char *)variant_get_type (value_v) == 'y')
    return variant_get_byte (value_v);
  return default_value;
}

static inline gint16
proposal_lookup_int16 (ProposalRef v, const char * key, gint16 default_value)
{
   VariantRef value_v;

  if (proposal_lookup (v, key, NULL, &value_v) &&
      *(const char *)variant_get_type (value_v) == 'n')
    return variant_get_int16 (value_v);
  return default_value;
}

static inline guint16
proposal_lookup_uint16 (ProposalRef v, const char * key, guint16 default_value)
{
   VariantRef value_v;

  if (proposal_lookup (v, key, NULL, &value_v) &&
      *(const char *)variant_get_type (value_v) == 'q')
    return variant_get_uint16 (value_v);
  return default_value;
}

static inline gint32
proposal_lookup_int32 (ProposalRef v, const char * key, gint32 default_value)
{
   VariantRef value_v;

  if (proposal_lookup (v, key, NULL, &value_v) &&
      *(const char *)variant_get_type (value_v) == 'i')
    return variant_get_int32 (value_v);
  return default_value;
}

static inline guint32
proposal_lookup_uint32 (ProposalRef v, const char * key, guint32 default_value)
{
   VariantRef value_v;

  if (proposal_lookup (v, key, NULL, &value_v) &&
      *(const char *)variant_get_type (value_v) == 'u')
    return variant_get_uint32 (value_v);
  return default_value;
}

static inline gint64
proposal_lookup_int64 (ProposalRef v, const char * key, gint64 default_value)
{
   VariantRef value_v;

  if (proposal_lookup (v, key, NULL, &value_v) &&
      *(const char *)variant_get_type (value_v) == 'x')
    return variant_get_int64 (value_v);
  return default_value;
}

static inline guint64
proposal_lookup_uint64 (ProposalRef v, const char * key, guint64 default_value)
{
   VariantRef value_v;

  if (proposal_lookup (v, key, NULL, &value_v) &&
      *(const char *)variant_get_type (value_v) == 't')
    return variant_get_uint64 (value_v);
  return default_value;
}

static inline guint32
proposal_lookup_handle (ProposalRef v, const char * key, guint32 default_value)
{
   VariantRef value_v;

  if (proposal_lookup (v, key, NULL, &value_v) &&
      *(const char *)variant_get_type (value_v) == 'h')
    return variant_get_handle (value_v);
  return default_value;
}

static inline double
proposal_lookup_double (ProposalRef v, const char * key, double default_value)
{
   VariantRef value_v;

  if (proposal_lookup (v, key, NULL, &value_v) &&
      *(const char *)variant_get_type (value_v) == 'd')
    return variant_get_double (value_v);
  return default_value;
}

static inline const char *
proposal_lookup_string (ProposalRef v, const char * key, const char * default_value)
{
   VariantRef value_v;

  if (proposal_lookup (v, key, NULL, &value_v) &&
      *(const char *)variant_get_type (value_v) == 's')
    return variant_get_string (value_v);
  return default_value;
}

static inline const char *
proposal_lookup_objectpath (ProposalRef v, const char * key, const char * default_value)
{
   VariantRef value_v;

  if (proposal_lookup (v, key, NULL, &value_v) &&
      *(const char *)variant_get_type (value_v) == 'o')
    return variant_get_objectpath (value_v);
  return default_value;
}

static inline const char *
proposal_lookup_signature (ProposalRef v, const char * key, const char * default_value)
{
   VariantRef value_v;

  if (proposal_lookup (v, key, NULL, &value_v) &&
      *(const char *)variant_get_type (value_v) == 'g')
    return variant_get_signature (value_v);
  return default_value;
}

static inline GString *
proposal_format (ProposalRef v, GString *s, gboolean type_annotate)
{
  gsize len = proposal_get_length (v);
  gsize i;

  if (len == 0 && type_annotate)
    g_string_append_printf (s, "@%s ", PROPOSAL_TYPESTRING);

  g_string_append_c (s, '{');
  for (i = 0; i < len; i++)
    {
      ProposalEntryRef entry = proposal_get_at (v, i);
      if (i != 0)
        g_string_append (s, ", ");
      __gstring_append_string (s, proposal_entry_get_key (entry));
      g_string_append (s, ": ");
      variant_format (proposal_entry_get_value (entry), s, type_annotate);
    }
  g_string_append_c (s, '}');
  return s;
}

static inline char *
proposal_print (ProposalRef v, gboolean type_annotate)
{
  GString *s = g_string_new ("");
  proposal_format (v, s, type_annotate);
  return g_string_free (s, FALSE);
}

/************** Results *******************/

static inline ResultsRef
results_from_gvariant (GVariant *v)
{
  g_assert (g_variant_type_equal (g_variant_get_type (v), RESULTS_TYPESTRING));
  return (ResultsRef) { g_variant_get_data (v), g_variant_get_size (v) };
}

static inline ResultsRef
results_from_bytes (GBytes *b)
{
  return (ResultsRef) { g_bytes_get_data (b, NULL), g_bytes_get_size (b) };
}

static inline ResultsRef
results_from_data (gconstpointer data, gsize size)
{
  return (ResultsRef) { data, size };
}

static inline GVariant *
results_dup_to_gvariant (ResultsRef v)
{
  guint8 *duped = _memdup2 (v.base, v.size);
  return g_variant_new_from_data (RESULTS_TYPEFORMAT, duped, v.size, TRUE, g_free, duped);
}

static inline GVariant *
results_to_gvariant (ResultsRef v,
                             GDestroyNotify      notify,
                             gpointer            user_data)
{
  return g_variant_new_from_data (RESULTS_TYPEFORMAT, v.base, v.size, TRUE, notify, user_data);
}

static inline GVariant *
results_to_owned_gvariant (ResultsRef v, GVariant *base)
{
  return results_to_gvariant (v, (GDestroyNotify)g_variant_unref, g_variant_ref (base));
}

static inline GVariant *
results_peek_as_gvariant (ResultsRef v)
{
  return g_variant_new_from_data (RESULTS_TYPEFORMAT, v.base, v.size, TRUE, NULL, NULL);
}

static inline ResultsRef
results_from_variant (VariantRef v)
{
  const GVariantType  *type;
  Ref child = variant_get_child (v, &type);
  g_assert (g_variant_type_equal(type, RESULTS_TYPESTRING));
  return results_from_data (child.base, child.size);
}

static inline gsize
results_get_length (ResultsRef v)
{
  if (v.size == 0)
    return 0;
  guint offset_size = ref_get_offset_size (v.size);
  gsize last_end = REF_READ_FRAME_OFFSET(v, 0);
  gsize offsets_array_size;
  if (last_end > v.size)
    return 0;
  offsets_array_size = v.size - last_end;
  if (offsets_array_size % offset_size != 0)
    return 0;
  gsize length  = offsets_array_size / offset_size;
  return length;
}

static inline ProposalRef
results_get_at (ResultsRef v, gsize index)
{
  guint offset_size = ref_get_offset_size (v.size);
  gsize last_end = REF_READ_FRAME_OFFSET(v, 0);
  gsize len = (v.size - last_end) / offset_size;
  gsize start = (index > 0) ? REF_ALIGN(REF_READ_FRAME_OFFSET(v, len - index), 8) : 0;
  G_GNUC_UNUSED gsize end = REF_READ_FRAME_OFFSET(v, len - index - 1);
  g_assert (start <= end);
  g_assert (end <= last_end);
  return (ProposalRef) { ((const char *)v.base) + start, end - start };
}

static inline GString *
results_format (ResultsRef v, GString *s, gboolean type_annotate)
{
  gsize len = results_get_length (v);
  gsize i;
  if (len == 0 && type_annotate)
    g_string_append_printf (s, "@%s ", RESULTS_TYPESTRING);
  g_string_append_c (s, '[');
  for (i = 0; i < len; i++)
    {
      if (i != 0)
        g_string_append (s, ", ");
      proposal_format (results_get_at (v, i), s, ((i == 0) ? type_annotate : FALSE));
    }
  g_string_append_c (s, ']');
  return s;
}

static inline char *
results_print (ResultsRef v, gboolean type_annotate)
{
  GString *s = g_string_new ("");
  results_format (v, s, type_annotate);
  return g_string_free (s, FALSE);
}

/************** Chunk *******************/

static inline ChunkRef
chunk_from_gvariant (GVariant *v)
{
  g_assert (g_variant_type_equal (g_variant_get_type (v), CHUNK_TYPESTRING));
  return (ChunkRef) { g_variant_get_data (v), g_variant_get_size (v) };
}

static inline ChunkRef
chunk_from_bytes (GBytes *b)
{
  return (ChunkRef) { g_bytes_get_data (b, NULL), g_bytes_get_size (b) };
}

static inline ChunkRef
chunk_from_data (gconstpointer data, gsize size)
{
  return (ChunkRef) { data, size };
}

static inline GVariant *
chunk_dup_to_gvariant (ChunkRef v)
{
  guint8 *duped = _memdup2 (v.base, v.size);
  return g_variant_new_from_data (CHUNK_TYPEFORMAT, duped, v.size, TRUE, g_free, duped);
}

static inline GVariant *
chunk_to_gvariant (ChunkRef v,
                             GDestroyNotify      notify,
                             gpointer            user_data)
{
  return g_variant_new_from_data (CHUNK_TYPEFORMAT, v.base, v.size, TRUE, notify, user_data);
}

static inline GVariant *
chunk_to_owned_gvariant (ChunkRef v, GVariant *base)
{
  return chunk_to_gvariant (v, (GDestroyNotify)g_variant_unref, g_variant_ref (base));
}

static inline GVariant *
chunk_peek_as_gvariant (ChunkRef v)
{
  return g_variant_new_from_data (CHUNK_TYPEFORMAT, v.base, v.size, TRUE, NULL, NULL);
}

static inline ChunkRef
chunk_from_variant (VariantRef v)
{
  const GVariantType  *type;
  Ref child = variant_get_child (v, &type);
  g_assert (g_variant_type_equal(type, CHUNK_TYPESTRING));
  return chunk_from_data (child.base, child.size);
}


static inline gsize
chunk_get_length (ChunkRef v)
{
  if (v.size == 0)
    return 0;
  guint offset_size = ref_get_offset_size (v.size);
  gsize last_end = REF_READ_FRAME_OFFSET(v, 0);
  gsize offsets_array_size;
  if (last_end > v.size)
    return 0;
  offsets_array_size = v.size - last_end;
  if (offsets_array_size % offset_size != 0)
    return 0;
  gsize length = offsets_array_size / offset_size;
  return length;
}

static inline ChunkEntryRef
chunk_get_at (ChunkRef v, gsize index)
{
  ChunkEntryRef res;
  guint offset_size = ref_get_offset_size (v.size);
  gsize last_end = REF_READ_FRAME_OFFSET(v, 0);
  gsize len = (v.size - last_end) / offset_size;
  gsize start = (index > 0) ? REF_ALIGN(REF_READ_FRAME_OFFSET(v, len - index), 8) : 0;
  gsize end = REF_READ_FRAME_OFFSET(v, len - index - 1);
  g_assert (start <= end);
  g_assert (end <= last_end);
  res = (ChunkEntryRef) { ((const char *)v.base) + start, end - start };
  return res;
}

static inline const char *
chunk_entry_get_key (ChunkEntryRef v)
{
  guint offset_size = ref_get_offset_size (v.size);
  G_GNUC_UNUSED gsize end = REF_READ_FRAME_OFFSET(v, 0);
  const char *base = (const char *)v.base;
  g_assert (end < v.size);
  g_assert (base[end-1] == 0);
  return base;
}

static inline VariantRef
chunk_entry_get_value (ChunkEntryRef v)
{
  guint offset_size = ref_get_offset_size (v.size);
  gsize end = REF_READ_FRAME_OFFSET(v, 0);
  gsize offset = REF_ALIGN(end, 8);
  g_assert (offset <= v.size);
  return (VariantRef) { (char *)v.base + offset, (v.size - offset_size) - offset };
}

static inline gboolean
chunk_lookup (ChunkRef v, const char * key, gsize *index_out, VariantRef *out)
{
  const char * canonical_key = key;
  if (v.size == 0)
    return FALSE;
  guint offset_size = ref_get_offset_size (v.size);
  gsize last_end = REF_READ_FRAME_OFFSET(v, 0);
  if (last_end > v.size)
    return FALSE;
  gsize offsets_array_size = v.size - last_end;
  if (offsets_array_size % offset_size != 0)
    return FALSE;
  gsize len = offsets_array_size / offset_size;
  gsize start = 0;
  gsize i;

  for (i = 0; i < len; i++)
    {
      gsize end = REF_READ_FRAME_OFFSET(v, len - i - 1);
      ChunkEntryRef e = { ((const guchar *)v.base) + start, end - start };
      g_assert (start <= end);
      g_assert (end <= last_end);
      const char * e_key = chunk_entry_get_key (e);
      if (strcmp(canonical_key, e_key) == 0)
        {
           if (index_out)
             *index_out = i;
           if (out)
             *out = chunk_entry_get_value (e);
           return TRUE;
        }
      start = REF_ALIGN(end, 8);
    }
    return FALSE;
}

static inline gboolean
chunk_lookup_boolean (ChunkRef v, const char * key, gboolean default_value)
{
   VariantRef value_v;

  if (chunk_lookup (v, key, NULL, &value_v) &&
      *(const char *)variant_get_type (value_v) == 'b')
    return variant_get_boolean (value_v);
  return default_value;
}

static inline guint8
chunk_lookup_byte (ChunkRef v, const char * key, guint8 default_value)
{
   VariantRef value_v;

  if (chunk_lookup (v, key, NULL, &value_v) &&
      *(const char *)variant_get_type (value_v) == 'y')
    return variant_get_byte (value_v);
  return default_value;
}

static inline gint16
chunk_lookup_int16 (ChunkRef v, const char * key, gint16 default_value)
{
   VariantRef value_v;

  if (chunk_lookup (v, key, NULL, &value_v) &&
      *(const char *)variant_get_type (value_v) == 'n')
    return variant_get_int16 (value_v);
  return default_value;
}

static inline guint16
chunk_lookup_uint16 (ChunkRef v, const char * key, guint16 default_value)
{
   VariantRef value_v;

  if (chunk_lookup (v, key, NULL, &value_v) &&
      *(const char *)variant_get_type (value_v) == 'q')
    return variant_get_uint16 (value_v);
  return default_value;
}

static inline gint32
chunk_lookup_int32 (ChunkRef v, const char * key, gint32 default_value)
{
   VariantRef value_v;

  if (chunk_lookup (v, key, NULL, &value_v) &&
      *(const char *)variant_get_type (value_v) == 'i')
    return variant_get_int32 (value_v);
  return default_value;
}

static inline guint32
chunk_lookup_uint32 (ChunkRef v, const char * key, guint32 default_value)
{
   VariantRef value_v;

  if (chunk_lookup (v, key, NULL, &value_v) &&
      *(const char *)variant_get_type (value_v) == 'u')
    return variant_get_uint32 (value_v);
  return default_value;
}

static inline gint64
chunk_lookup_int64 (ChunkRef v, const char * key, gint64 default_value)
{
   VariantRef value_v;

  if (chunk_lookup (v, key, NULL, &value_v) &&
      *(const char *)variant_get_type (value_v) == 'x')
    return variant_get_int64 (value_v);
  return default_value;
}

static inline guint64
chunk_lookup_uint64 (ChunkRef v, const char * key, guint64 default_value)
{
   VariantRef value_v;

  if (chunk_lookup (v, key, NULL, &value_v) &&
      *(const char *)variant_get_type (value_v) == 't')
    return variant_get_uint64 (value_v);
  return default_value;
}

static inline guint32
chunk_lookup_handle (ChunkRef v, const char * key, guint32 default_value)
{
   VariantRef value_v;

  if (chunk_lookup (v, key, NULL, &value_v) &&
      *(const char *)variant_get_type (value_v) == 'h')
    return variant_get_handle (value_v);
  return default_value;
}

static inline double
chunk_lookup_double (ChunkRef v, const char * key, double default_value)
{
   VariantRef value_v;

  if (chunk_lookup (v, key, NULL, &value_v) &&
      *(const char *)variant_get_type (value_v) == 'd')
    return variant_get_double (value_v);
  return default_value;
}

static inline const char *
chunk_lookup_string (ChunkRef v, const char * key, const char * default_value)
{
   VariantRef value_v;

  if (chunk_lookup (v, key, NULL, &value_v) &&
      *(const char *)variant_get_type (value_v) == 's')
    return variant_get_string (value_v);
  return default_value;
}

static inline const char *
chunk_lookup_objectpath (ChunkRef v, const char * key, const char * default_value)
{
   VariantRef value_v;

  if (chunk_lookup (v, key, NULL, &value_v) &&
      *(const char *)variant_get_type (value_v) == 'o')
    return variant_get_objectpath (value_v);
  return default_value;
}

static inline const char *
chunk_lookup_signature (ChunkRef v, const char * key, const char * default_value)
{
   VariantRef value_v;

  if (chunk_lookup (v, key, NULL, &value_v) &&
      *(const char *)variant_get_type (value_v) == 'g')
    return variant_get_signature (value_v);
  return default_value;
}

static inline GString *
chunk_format (ChunkRef v, GString *s, gboolean type_annotate)
{
  gsize len = chunk_get_length (v);
  gsize i;

  if (len == 0 && type_annotate)
    g_string_append_printf (s, "@%s ", CHUNK_TYPESTRING);

  g_string_append_c (s, '{');
  for (i = 0; i < len; i++)
    {
      ChunkEntryRef entry = chunk_get_at (v, i);
      if (i != 0)
        g_string_append (s, ", ");
      __gstring_append_string (s, chunk_entry_get_key (entry));
      g_string_append (s, ": ");
      variant_format (chunk_entry_get_value (entry), s, type_annotate);
    }
  g_string_append_c (s, '}');
  return s;
}

static inline char *
chunk_print (ChunkRef v, gboolean type_annotate)
{
  GString *s = g_string_new ("");
  chunk_format (v, s, type_annotate);
  return g_string_free (s, FALSE);
}

/************** Chunks *******************/

static inline ChunksRef
chunks_from_gvariant (GVariant *v)
{
  g_assert (g_variant_type_equal (g_variant_get_type (v), CHUNKS_TYPESTRING));
  return (ChunksRef) { g_variant_get_data (v), g_variant_get_size (v) };
}

static inline ChunksRef
chunks_from_bytes (GBytes *b)
{
  return (ChunksRef) { g_bytes_get_data (b, NULL), g_bytes_get_size (b) };
}

static inline ChunksRef
chunks_from_data (gconstpointer data, gsize size)
{
  return (ChunksRef) { data, size };
}

static inline GVariant *
chunks_dup_to_gvariant (ChunksRef v)
{
  guint8 *duped = _memdup2 (v.base, v.size);
  return g_variant_new_from_data (CHUNKS_TYPEFORMAT, duped, v.size, TRUE, g_free, duped);
}

static inline GVariant *
chunks_to_gvariant (ChunksRef v,
                             GDestroyNotify      notify,
                             gpointer            user_data)
{
  return g_variant_new_from_data (CHUNKS_TYPEFORMAT, v.base, v.size, TRUE, notify, user_data);
}

static inline GVariant *
chunks_to_owned_gvariant (ChunksRef v, GVariant *base)
{
  return chunks_to_gvariant (v, (GDestroyNotify)g_variant_unref, g_variant_ref (base));
}

static inline GVariant *
chunks_peek_as_gvariant (ChunksRef v)
{
  return g_variant_new_from_data (CHUNKS_TYPEFORMAT, v.base, v.size, TRUE, NULL, NULL);
}

static inline ChunksRef
chunks_from_variant (VariantRef v)
{
  const GVariantType  *type;
  Ref child = variant_get_child (v, &type);
  g_assert (g_variant_type_equal(type, CHUNKS_TYPESTRING));
  return chunks_from_data (child.base, child.size);
}

static inline gsize
chunks_get_length (ChunksRef v)
{
  if (v.size == 0)
    return 0;
  guint offset_size = ref_get_offset_size (v.size);
  gsize last_end = REF_READ_FRAME_OFFSET(v, 0);
  gsize offsets_array_size;
  if (last_end > v.size)
    return 0;
  offsets_array_size = v.size - last_end;
  if (offsets_array_size % offset_size != 0)
    return 0;
  gsize length  = offsets_array_size / offset_size;
  return length;
}

static inline ChunkRef
chunks_get_at (ChunksRef v, gsize index)
{
  guint offset_size = ref_get_offset_size (v.size);
  gsize last_end = REF_READ_FRAME_OFFSET(v, 0);
  gsize len = (v.size - last_end) / offset_size;
  gsize start = (index > 0) ? REF_ALIGN(REF_READ_FRAME_OFFSET(v, len - index), 8) : 0;
  G_GNUC_UNUSED gsize end = REF_READ_FRAME_OFFSET(v, len - index - 1);
  g_assert (start <= end);
  g_assert (end <= last_end);
  return (ChunkRef) { ((const char *)v.base) + start, end - start };
}

static inline GString *
chunks_format (ChunksRef v, GString *s, gboolean type_annotate)
{
  gsize len = chunks_get_length (v);
  gsize i;
  if (len == 0 && type_annotate)
    g_string_append_printf (s, "@%s ", CHUNKS_TYPESTRING);
  g_string_append_c (s, '[');
  for (i = 0; i < len; i++)
    {
      if (i != 0)
        g_string_append (s, ", ");
      chunk_format (chunks_get_at (v, i), s, ((i == 0) ? type_annotate : FALSE));
    }
  g_string_append_c (s, ']');
  return s;
}

static inline char *
chunks_print (ChunksRef v, gboolean type_annotate)
{
  GString *s = g_string_new ("");
  chunks_format (v, s, type_annotate);
  return g_string_free (s, FALSE);
}
