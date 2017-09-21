# Beautifier plugin usage:

To be able to trigger a beautify action you need:

- Act on a file with a recognized language (or change the language in Builder interface)
- Have a command configured for this language (or the menu will be empty)
- The command executable need to be reachable from your $PATH
- if your command use a config file, it need to be present.
- Some text need to be selected.

Then, two possible actions:
- A default entry is defined: pressing &lt;ctrl&gt;&lt;alt&gt;b trigger the default beautify action.
- You can manually choose an entry in the contextual menu at "Selection-> Beautify" submenu.

# Beautifier plugin configuration :

Every configuration files use the GKeyFile GLib format:
https://developer.gnome.org/glib/unstable/glib-Key-value-file-parser.html

The beautifier plugin has three layers of configuration:
- user wide:

  ~/.config/gnome-builder/beautifier_plugin

- project wide:

  .beautifier folder at project root.

- system wide, installed by gnome-builder:

  $PREFIX/share/gnome-builder/plugins/beautifier_plugin

Each layer has a priority in the same order:
- user
- project
- system

This count for the profiles, and for the default entry too.

In each layer you will find:
- a global.ini file.
- any number of languages (using GtkSourceView lang_id) or group-of-languages (mapped in global.ini) folders.

In each language or group-of-languages folder, you will find:
- A config.ini
- Any number of configuration files used by your beautifier commands.

```
	[layer root]
		global.ini
		[lang_id folder]
			config.ini
			a_command_config_file.cfg (or any other file extension)
			another_config_file.cfg
		[mapped folder]
			config.ini
			a_command_config_file.cfg (or any other file extension)
			yet_another_config_file.cfg
```

## global.ini groups and keys:

The global.ini file define options for a specific language.
The group name need be one of the GtkSourveView language id (lang_id).

lang_ids are the same used by the language-specs GtkSourceView .lang files:
https://git.gnome.org/browse/gtksourceview/tree/data/language-specs

```
	[c]
	[java]
	[javascript]

	etc...
```

The possible keys are:
- map = language or group-of-languages folder name for the layer.
- default = default profile to get the accelerator (<ctrl><alt>b)

This permit to reference the same folder from different languages:

```
	[chdr]
	map = c_family
	default = k&r

	[objc]
	map = c_family
```

Inside a group, mandatory keys are: map

## config.ini groups and keys:

The config.ini file define configuration to display and launch a beaufity action.
Groups are freely named (respecting GKeyFile syntax).
The keys are:

- command = the command to launch (currently clang-format, because of the special processing needed)

or

- command-pattern = commandline to launch (starting by the command name and followed by possible arguments)
  with pattern substitution:
      @s@ for the selected text put in a file.
      @c@ for the config file define by the config key.

By adding [internal] in front of the command pattern, the command is
searched in the Builder data dir.

- name = the real name to display in the menu

- config = the config file name (located in the same folder as the config.ini file)

Mandatory key are: name and command or command-pattern, config if command-pattern need it.

A specific group named [global] allow folder wide configuration.
Currently, there's only one key: default = default profile name, the one that gets the accelerator.

## Example of configuration:

```
Files:
	[layer root]
		global.ini
		c_family
			config.ini
			k&r-uncrustify.cfg
			gnu-uncrustify.cfg

		c
			config.ini
			gnome-builder-clang.cfg

Content:
	global.ini:
		[chdr]
		map = c_family
		default = k&r

		[objc]
		map = c_family

	c_family/config.ini:
		[global]
		default = gnu

		[k&r]
		command-pattern = uncrustify -c @c@ -f @s@
		config = k&r-uncrustify.cfg
		name = Kernighan and Ritchie

		[gnu]
		command-pattern = uncrustify -c @c@ -f @s@
		config = gnu-uncrustify.cfg
		name = Gnu Style

	c/config.ini:
		[global]
		default = my_config

		[my_config]
		command = clang-format
		config = gnome-builder-clang.cfg
		name = my Clang style
```
