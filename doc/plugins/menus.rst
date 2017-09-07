###########################
Extending Application Menus
###########################

Menus in Builder are implemented using the ``GtkBuilder`` based menu definitions.
These are created in ``gtk/menus.ui`` files within your extensions resources.

Menus are automatically merged into the application when your plugin is loaded.
If the plugin is unloaded by the user, they will be automatically unloaded as well.

Extended Menu Features
======================

The ``GMenu`` abstraction is somewhat limited in features that Builder requires.
In particular, Builder needs advanced sorting, icons, and accelerators for menu items.
To overcome this, Builder has special "menu merging" code which has been extrated into ``libdazzle`` (a utility library).

Here is an example of how the valgrind plugin extends the Run menu.

.. code-block:: xml
   :caption: Embed file as a resource matching /org/gnome/Builder/plugins/valgrind_plugin/gtk/menus.ui

   <?xml version="1.0" encoding="UTF-8"?>
   <interface>
     <!-- "run-menu" is the unique name of the run menu -->
     <menu id="run-menu">
       <!-- The menu has sections in it, just like GMenu -->
       <section id="run-menu-section">
         <item>
           <!-- by specifying an id, we can position other items relatively -->
           <attribute name="id">valgrind-run-handler</attribute>

           <!-- after/before can be used to position this menu relative to others -->
           <attribute name="after">default-run-handler</attribute>

           <!-- the GAction name and action target to activate -->
           <attribute name="action">run-manager.run-with-handler</attribute>
           <attribute name="target">valgrind</attribute>

           <!-- The label to display -->
           <attribute name="label" translatable="yes">Run with Valgrind</attribute>

           <!-- An "icon-name" to show if icons are to be visible in the menu -->
           <attribute name="verb-icon-name">system-run-symbolic</attribute>

           <!-- An accelerator to display if accelerators are to be displayed -->
           <attribute name="accel">&lt;Control&gt;F10</attribute>
         </item>
       </section>
     </menu>
   </interface>

For more information on embedding resources with Python-based plugins,
see :ref:`creating embedded GResources<embedding_resources>`.

Accessing Merged Menus
======================

Merged menus are available via the ``Dzl.Application`` base class of ``Ide.Application``.

.. code-block:: python3

   menu = Ide.Application.get_default().get_menu_by_id('run-menu')

