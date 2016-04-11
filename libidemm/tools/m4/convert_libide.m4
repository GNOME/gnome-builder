dnl Enums
_CONV_ENUM(Ide,ApplicationMode)
_CONV_ENUM(Ide,UriParseFlags)
_CONV_ENUM(Ide,UriToStringFlags)

dnl Glibmm Enums
#_CONV_GLIB_ENUM(ThreadPriority)


dnl ############### libidemm Class Conversions ######################


dnl ##################General Conversions############################

dnl Basic General Conversions
_CONVERSION(`guint64&',`guint64*',`&$3')
_CONVERSION(`guint*',`guint&',`*$3')
_CONVERSION(`const guint&',`guint',`$3')
_CONVERSION(`gsize*',`gsize&',`*$3')
_CONVERSION(`const guint32&',`guint32',`$3')
_CONVERSION(`guint8*&',`guint8**',`&$3')
_CONVERSION(`gdouble&',`gdouble*',`&$3')

dnl Glibmm Conversions
_CONVERSION(`const Glib::Error&', `const GError*', `$3.gobj()')
_CONVERSION(`GQuark',`Glib::QueryQuark',`Glib::QueryQuark($3)')
_CONVERSION(`const Glib::QueryQuark&',`GQuark',`$3')
_CONVERSION(`Glib::Threads::RecMutex&',`GRecMutex*',`$3.gobj()')
_CONVERSION(`const Glib::StringArrayHandle&',`const gchar**',`const_cast<const char**>($3.data())')
_CONVERSION(`const Glib::ValueArray&',`GValueArray*',`const_cast<GValueArray*>($3.gobj())')

dnl String Conversions

_CONVERSION(`const std::string&',`const guchar*',`($2)($3.c_str())')
_CONVERSION(`gchar*',`const Glib::ustring&',__GCHARP_TO_USTRING)
_CONVERSION(`gchararray',`const Glib::ustring&',__GCHARP_TO_USTRING)
_CONVERSION(`const gchar*',`const Glib::ustring&',__GCHARP_TO_USTRING)
_CONVERSION(`Glib::ustring&',`const guchar*', ($2)($3.c_str()))
_CONVERSION(`const Glib::ustring&',`gchararray', $3.c_str())

dnl Ide::RecentProjects
_CONVERSION(`IdeRecentProjects*',`Glib::RefPtr<Ide::RecentProjects>',`Glib::wrap($3)')

dnl Ide::Application
_CONVERSION(`const Glib::RefPtr<Ide::Application>&',`IdeApplication*',__CONVERT_REFPTR_TO_P)
_CONVERSION(`IdeApplication*',`const Glib::RefPtr<Ide::Application>&',Glib::wrap($3, true))

dnl Ide::Object
_CONVERSION(`IdeObject*',`Glib::RefPtr<Ide::Object>',Glib::wrap($3, true))

dnl Ide::BackForwardList
_CONVERSION(`IdeBackForwardList*',`Glib::RefPtr<Ide::BackForwardList>',Glib::wrap($3))
_CONVERSION(`const Glib::RefPtr<Ide::BackForwardList>&',`IdeBackForwardList*',__CONVERT_REFPTR_TO_P)
