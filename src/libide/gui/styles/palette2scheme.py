#!/usr/bin/env python3

import gi
import sys

gi.require_version('Gdk', '4.0')

from gi.repository import Gdk
from gi.repository import GLib

def get_color(key_file, group, key):
    string = key_file.get_string(group, key)
    rgba = Gdk.RGBA()
    rgba.parse(string)
    return rgba

def premix(bg, fg, alpha):
    ret = Gdk.RGBA()
    ret.red = ((1 - alpha) * bg.red) + (alpha * fg.red);
    ret.green = ((1 - alpha) * bg.green) + (alpha * fg.green);
    ret.blue = ((1 - alpha) * bg.blue) + (alpha * fg.blue);
    ret.alpha = 1.0;
    return ret

def html(rgba):
    return '#%02x%02x%02x' % (int(rgba.red * 255), int(rgba.green * 255), int(rgba.blue * 255))

def do_scheme(key_file, group, has_alt, _name):
    gstr = GLib.String()

    if group != 'Palette':
        _name += ' ' + group

    _id = _name.lower().replace(' ', '-')

    Color0 = get_color(key_file, group, 'Color0')
    Color1 = get_color(key_file, group, 'Color1')
    Color2 = get_color(key_file, group, 'Color2')
    Color3 = get_color(key_file, group, 'Color3')
    Color4 = get_color(key_file, group, 'Color4')
    Color5 = get_color(key_file, group, 'Color5')
    Color6 = get_color(key_file, group, 'Color6')
    Color7 = get_color(key_file, group, 'Color7')
    Color8 = get_color(key_file, group, 'Color8')
    Color9 = get_color(key_file, group, 'Color9')
    Color10 = get_color(key_file, group, 'Color10')
    Color11 = get_color(key_file, group, 'Color11')
    Color12 = get_color(key_file, group, 'Color12')
    Color13 = get_color(key_file, group, 'Color13')
    Color14 = get_color(key_file, group, 'Color14')
    Color15 = get_color(key_file, group, 'Color15')
    Foreground = get_color(key_file, group, 'Foreground')
    Background = get_color(key_file, group, 'Background')

    try:
        Cursor = get_color(key_file, group, 'Cursor')
    except:
        Cursor = Foreground

    Comment = premix(Background, Foreground, .7)
    Gutter = premix(Background, Foreground, .25)
    CurrentLine = premix(Background, Foreground, .05)

    gstr.append('<?xml version="1.0" encoding="UTF-8"?>\n')
    gstr.append('<!-- This file was generated from https://github.com/Gogh-Co/Gogh/tree/master/themes/ -->\n')
    gstr.append('<style-scheme id="%s" name="%s" version="1.0">\n' % (_id, _name))
    gstr.append('\n')
    for Color in ('Foreground', 'Background', 'Cursor', 'Gutter', 'CurrentLine'):
        gstr.append('  <color name="%s" value="%s"/>\n' % (Color, html(locals()[Color])))
    for i in range(0, 16):
        gstr.append('  <color name="Color%d" value="%s"/>\n' % (i, html(locals()['Color'+str(i)])))
    gstr.append('\n')
    gstr.append('  <style name="text" background="Background" foreground="Foreground"/>\n')
    gstr.append('  <style name="cursor" foreground="Cursor"/>\n')
    gstr.append('  <style name="search-match" background="Color3" foreground="Background"/>\n')
    gstr.append('  <style name="line-numbers" background="Background" foreground="Gutter"/>\n')
    gstr.append('  <style name="current-line" background="CurrentLine"/>\n')
    gstr.append('  <style name="current-line-number" background="CurrentLine"/>\n')
    gstr.append('  <style name="right-margin" foreground="Foreground" background="Gutter"/>\n')
    gstr.append('\n')
    gstr.append('  <style name="def:base-n-integer" foreground="Color5"/>\n')
    gstr.append('  <style name="def:boolean" foreground="Color5"/>\n')
    gstr.append('  <style name="def:character" foreground="Color5"/>\n')
    gstr.append('  <style name="def:comment" foreground="%s"/>\n' % html(Comment))
    gstr.append('  <style name="def:doc-comment-element" bold="true"/>\n')
    gstr.append('  <style name="def:constant" foreground="Color5"/>\n')
    gstr.append('  <style name="def:decimal" foreground="Color5"/>\n')
    gstr.append('  <style name="def:error" underline="error" underline-color="Color1"/>\n')
    gstr.append('  <style name="def:floating-point" foreground="Color5"/>\n')
    gstr.append('  <style name="def:keyword" foreground="Color2"/>\n')
    gstr.append('  <style name="def:net-address" foreground="Color4" underline="low"/>\n')
    gstr.append('  <style name="def:function" foreground="Color4"/>\n')
    gstr.append('  <style name="def:number" foreground="Color5"/>\n')
    gstr.append('  <style name="def:preprocessor" foreground="Color5"/>\n')
    gstr.append('  <style name="def:special-char" foreground="Color4"/>\n')
    gstr.append('  <style name="def:string" foreground="Color3"/>\n')
    gstr.append('  <style name="def:strong-emphasis" bold="true"/>\n')
    gstr.append('  <style name="def:identifier" foreground="Color2"/>\n')
    gstr.append('  <style name="def:type" foreground="Color2"/>\n')
    gstr.append('\n')
    gstr.append('</style-scheme>')

    print(gstr.str)

for file in sys.argv[1:]:
    key_file = GLib.KeyFile.new()
    key_file.load_from_file(file, 0)

    name = key_file.get_string('Palette', 'Name')

    has_light = key_file.has_group('Light')
    has_dark = key_file.has_group('Dark')

    if has_light:
        do_scheme(key_file, 'Light', has_dark, name)

    if has_dark:
        do_scheme(key_file, 'Dark', has_light, name)

    if not (has_light or has_dark):
        do_scheme(key_file, 'Palette', False, name)

