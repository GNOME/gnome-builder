#TODO list:

## Enhancements :

### Palettes :
- use need attention symbol in changed palettes.
- show ID and number of colors (tooltip ?)

### Save and load palettes dialogs :
- load more than one palette at once.
- add button the choose the save format

### Panel :
- show complex colors and nested ones: see css parser functions handling.
- How to close it ?
- activated globaly (but nedd a color sub-menu) or per view like now ?
- filter prefs page ?

### Color Strings :
- add a way to choose a reference format to insert in the view ( label as a radio ? ).

### search list :
- add current file colors to the list.

### Prefs :
- separate color components and color strings unit choices.

### Assets :
- fix python script (till now, the rubby one is used).
- percent and degree icons are fuzzy.

## FIXES :

### Prefs :
- checked palette row with long name make the panel ask more width:
  partialy fixed, but hard problem :
    if you select a row with a row name larger than the previously selected and
    the panel has small width, it make the panel grow and shrink when changing rows.

- add some scrolledwindow to the pages :
  done for the palette list but there's a size problem somewhere since
  the addition of max-content-* to GtkScrolledWindow.

- when adding new palettes, the bottom list border sometimes disappears.
( css problem ?)

- focus paths.
