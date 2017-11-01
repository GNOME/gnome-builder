###################################
Registering Application Preferences
###################################

Builder's preferences are designed in such a way that the core application preferences and extension preferences feel cohesive.
This is often not the case with "plugin-based" applications.

To do this, we created the ``Ide.Preferences`` interface.
Extensions can implement the ``Ide.PreferencesAddin`` interface and register preferences with the ``Ide.Preferences`` instance.
This allows the preferences to be seamlessly merged together based on the desired page, group, and preference type.
They are even searchable!


.. code-block:: python3

   # my_plugin.py

   import gi

   from gi.repository import GObject
   from gi.repository import Ide

   _ = Ide.gettext

   class MyPreferencesAddin(GObject.Object, Ide.PreferencesAddin):

       def do_load(self, prefs):

           # add a new switch row
           self.completion_id = prefs.add_switch(
                   # to the code-insight page
                   'code-insight',

                   # in the completion group
                   'completion',

                   # mapping to the gsettings schema
                   'org.gnome.builder.extension-type',

                   # with the gsettings schema key
                   'enabled',

                   # And the gsettings path
                   '/',

                   # The target GVariant value if necessary (usually not)
                   None,

                   # title
                   _("Suggest Python completions"),

                   # subtitle
                   _("Use Jedi to provide completions for the Python language"),

                   # keywords
                   _("jedi python search autocompletion API"),
                   
                   # with sort priority
                   30)

           # there are plenty of other types of things you can add.
           # see dzl-preferences.h for an example of the API you can call
           # such as:
           #
           #  - add_group()
           #  - add_list_group()
           #  - add_font_button()
           #  - add_radio()
           #  - add_spin_button()
           #  - add_file_chooser()
           #  - add_custom() (custom widgets)

       def do_unload(self, prefs):
           prefs.remove_id(self.completion_id)

