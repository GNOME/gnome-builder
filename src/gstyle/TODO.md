#TODO list :

## Enhancements :

### Color :
- add HSL, HWB and CMYK.
- add illuminant management.

### Color widget :
- fix border-radius calculation, result different than the one used by gtk+.

### Color plane :
- add circle cursor.
- icon for out-of-gamut color.
- use filtering framework (reduce set of colors like websafe and colorblindness amongst others)

### Panel :
- add a horizontal displayed mode ?
- add a mini-picker mode to be displayed in a popover at cursor position.

### Palettes :
- add more load/save formats :
  - including the gtksourceview style scheme : filter the unneeded part but keep it for save it again.
- allow scroll when drag at the end of list (both list or flow mode).

### Palette widget :
- add a placeholder when showing an empty palette
 (should react to dnd)

### CSS parser :
- add color functions parsing:
  - darker, lighter, mix, alpha.

### Sort :
- sort by various mode:
  - by name.
  - by light.
  - by hue.

- sort relative to the selected color:
  - by approching color deltaE.2000 (calculation already in gstyle)

  Do sorting change the saved order of colors or just the view ?

### Drag'n drop :
- dnd-lock: a lock-symbolic lock icon.

### Slidein :
- is it possible to not close the slidein when dragging the paned border to extend the panel ?

### Color plane :
- fix cursor visibility on border when using the sliders.

### Tests :
- make them real test, not just result output.
- fix panel test to use prefs pages.
- fix palette widget test to be able to select a palette.

## FIXES :

### Theme :
- add backdrop state.
