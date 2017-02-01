ruler
=====

Design Document
---------------

ruler is a program for creating window "rules" like in other window managers.
The users writes the rules in a config file. The program executes commands when
windows with a specific name/class are created. For example, all windows in the
`Firefox` class can be moved to workspace 2 with a command.

### Config file syntax

```
DESCRIPTOR
	[;]COMMAND
```

```
block = descriptor, "\n", command
      | descriptor, "\n", command, "\n", block;
descriptor = var
           | var, white space, descriptor;
command = all characters
        | ";", all characters;
var = criterion, "=", value;
value = '"', {all characters - '"'}, '"';
criterion = "class" | "instance" | "type" | "title" | "role";
all characters = ? all printable characters ? ;
```

`value` and `command` can contain escaped characters, preceded by a `\`.

`value` can be a regular expression (POSIX extended regular expression).

If `command` is preceded by a `;`, the command will be run synchronously,
otherwise it will be run asynchronously.

`command` will be executed by the shell set in the `SHELL` environment
variable. The window id will be set in the `RULER_ID` environment variable.

`criterion` can be:

* `class` - the second part of `WM_CLASS` window property.
* `instance` - the first part of `WM_CLASS` window property.
* `type` - the `_NET_WM_WINDOW_TYPE` window property. `VALUE` can be
	`desktop`, `dock`, `toolbar`, `menu`, `utility`, `splash`, `dialog`,
	`dropdown_menu`, `popup_menu`, `tooltip`, `notification`, `combo`, `dnd`,
	`normal`.
* `title` - the X11 window title (`_NET_WM_NAME` and `WM_NAME` properties, the
	latter used as a fallback).
* `role` - window role (`WM_WINDOW_ROLE` property)

### Example

```
role="browser"
	waitron group_add_window 2
```

Implementation details
----------------------

* Ruler is written in POSIX C (define `_POSIX_C_SOURCE`)
* Ruler will make use of the XCB and XCB Util WM libraries.
* Regular expressions implementation is [POSIX](https://www.gnu.org/software/libc/manual/html_node/Regular-Expressions.html)
