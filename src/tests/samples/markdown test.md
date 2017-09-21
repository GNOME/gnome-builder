### Headings:
---

# Heading H1
Heading H1
=========

## Heading H2
Heading H2
-------------

### Heading H3
#### Heading H4
##### Heading H5
###### Heading H6

Hashes at the end are ignored :
#### New Heading ####


### Heading with ~~forma~~*tting*

### Paragraphs:
---

First paragraph first line
First paragraph same line

Second paragraph first line

Second paragraph second line

### Character styles:
---

Regular characters

*Italic characters* _Italic characters_
**Bold characters** __Bold characters__

*nested **styles** *

**nested *styles* **

### Lists:
---

* Item 1
- Item 2
 * Item 21
       + Item 21 sentence 1. Item 21 sentence 2.

         Item 21 second paragraph.

         Support other elements:
         ###### Heading
         a [link](http://gnome.org "gnome"),
         ~~strickthough~~,
         `code`,
         ![gnome logo](gnome-logo.png)
         > quote

 * Item 22
* Item 3

1. Item 1
2. Item 2
  * item 21
    - item 22

      item text
      on multi-lines

3. Item 3
    1. Item 31
    2. Item 32
    3. Item 33

### Quotes:
---
Simple quote:

> quote first line
> quote second line

Multi-lines quote:

> quote start

> quote end

Nested quote:

>> nested
>>
>>> on multiple
>>
>> levels

With other elements:

> ## Heading
> a [link](http://gnome.org "gnome"),
> ~~strickthough~~,
> `code`,
> ![gnome logo](gnome-logo.png),
> + item
> + Item

### Code block and inline code:
---

	a multi-line
	  idented line
	  <div class="md">escaped html tags</div>
	code block

and

	A very long code block to show a slider in the Web view - a very long code block to show a slider in the web view - a very long code block to show a slider in the web view

Some `inlined code` , even `underscored_function_names` then ``a escaped ` backtick``

## Links:
---

A [link with tittle](http://gnome.org "gnome")

A [link](http://gnome.org)

A [referenced link][1] or [Gnome][]

A local link [link](markdown test page 2.html)

An implicit link: <http://gnome.org> or <address@example.com>

[1]: http://gnome.org "gnome"
[gnome]: http://gnome.org "Gnome"

## Notes:
---

*Note: a note*

## Images:
---

Remote: ![remote gnome balloon](https://www.gnome.org/wp-content/uploads/2010/09/foundation_balloon.png)
Local with title: ![local gnome logo with tittle](gnome-logo.png "gnome logo")
Local no title: ![local gnome logo](gnome-logo.png)
Referenced: ![referenced gnome logo][2]

With links:
[<img src="gnome-logo.png">](http://gnome.org) or [![Gnome](gnome-logo.png)](http://gnome.org)


[2]: gnome-logo.png "gnome logo"

## Tables:
---

|col 1|col 2|col 3|
|-|-|-|
|1|2|3|
|4|5|6|
|7|8|9|

## Escaped characters:
---

\\ \` \* \_

\{ \} \( \) \[ \]

\. \!

## GitHub flavored markdown:
---

underscore_separated_words

auto url: http://gnome.org

~~strikethrough characters~~

work with nested too:

*nested **styles** ~~strickethough~~ * and **nested styles ~~strickethough~~ **

# Fence code block:

```
A code block
   with escaped characters \t \r \n

more code
```

With syntax highlight (need highlighter):

```c
printf ("just a test\n");
/* comment */
```

# Tables:

|col 1|col 2
|-|-
|a [link](http://gnome.org "gnome")|~~strickthough~~
|*italic*|**bold**
|`code`|![gnome logo](gnome-logo.png)

|left|center|right|
|:---|:----:|----:|
|left-aligned|centered|right-aligned|

# Rules:

---
- - -
***
* * *
___
_ _ _


# Raw html:

<h3>Document Title</h3>

<ul>
  <li>
    <p>❏ todo</p>
  </li>
  <li>
    <p>✓ done</p>
  </li>
</ul>

<table>
  <tr>
    <td>
      <div>First column</div>
    </td>
    <td>
      Second column
    </td>
  </tr>
</table>

<pre lang="c">
  <code>printf ("Hello");</code>
</pre>

