#include <ctype.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>

#include <xcb/xcb.h>
#include <xcb/xcb_icccm.h>
#include <xcb/xcb_ewmh.h>
#include <wm.h>

#include "ruler.h"

extern FILE * yyin;
struct list *last_d = NULL;
struct list *block_list = NULL;
command_t last_c;

xcb_connection_t *conn;
xcb_screen_t *scrn;
xcb_ewmh_connection_t *ewmh;

void
print_usage(const char *program_name)
{
	fprintf(stderr, "Usage: %s <filename>\n", program_name);
	exit(1);
}

char *
strip_quotes(char *str)
{
	/* length of original string - 2 (two quotes) + 1 (a null terminator) */
	int len = strlen(str) - 2;

	/* check if string needs to be stripped */
	if (str[0] != '"' || str[len + 1] != '"')
		return str;

	char *s = malloc((len + 1) * sizeof(char));
	int i;

	for (i = 1; i < strlen(str) - 1; i++) {
		s[i - 1] = str[i];
	}
	s[len] = '\0';
	free(str);

	return s;
}

struct descriptor *
new_descriptor(char *criterion, char *str)
{
	struct descriptor *d = malloc(sizeof(struct descriptor));
	int status;
	str = strip_quotes(str);

	/* convert criterion from string form to enum form */
#define MATCH_CRIT(c, C) if (strcmp(criterion, #c) == 0) d->criterion = CRIT_##C
	MATCH_CRIT(class, CLASS);
	MATCH_CRIT(instance, INSTANCE);
	MATCH_CRIT(type, TYPE);
	MATCH_CRIT(name, NAME);
	MATCH_CRIT(role, ROLE);
#undef MATCH_CRIT

	d->str = str;
	d->reg = malloc(sizeof(regex_t));

	fprintf(stderr, "new regex from `%s`\n", str);
	status = regcomp(d->reg, str, REGEX_FLAGS);
	if (status != 0) {
		warnx("couldn't compile regex for %s=\"%s\"", criterion, str);
		regfree(d->reg);
		free(d->reg);
		d->reg = NULL;
	}

	return d;
}

char *
criterion_to_string(enum criterion c)
{
	char *s;
	switch (c) {
		case CRIT_CLASS: s = "class"; break;
		case CRIT_INSTANCE: s = "instance"; break;
		case CRIT_TYPE: s = "type"; break;
		case CRIT_NAME: s = "name"; break;
		case CRIT_ROLE: s = "role"; break;
	}

	return strdup(s);
}

/*
 * Create descriptor and add it to the list.
 */
void
desc(char *crit, char *str)
{
	struct descriptor *d = new_descriptor(crit, str);
	list_add(&last_d, d);
}

/*
 * Free descriptor.
 */
void descriptor_free(struct descriptor *d)
{
	free(d->str);
	free(d);
}

/*
 * Add node to the back of the list.
 */
void
list_add(struct list **list, void *n)
{
	struct list *to_alloc;

	if (*list == NULL)
		to_alloc = *list;
	else
		to_alloc = (*list)->prev;

	to_alloc = malloc(sizeof(struct list));
	to_alloc->n = n;

	if (*list == NULL) {
		to_alloc->next = to_alloc->prev = NULL;
	} else {
		to_alloc->next = *list;
		(*list)->prev = to_alloc;
		to_alloc->prev = NULL;
	}

	*list = to_alloc;
}

/*
 * Free all nodes of the list.
 */
void
list_free(struct list **list)
{
	struct list *d, *next;
	for (d = *list; d != NULL; d = next) {
		next = d->next;
		free(d);
	}

	*list = NULL;
}

command_t
new_command(char *comm)
{
	return comm;
}

void
comm(char *comm)
{
	command_t c = new_command(comm);
	last_c = c;
}

/*
 * Create a new block from a list of descriptors and a command.
 */
struct block *
new_block(struct list *d, command_t c)
{
	struct block *b = malloc(sizeof(struct block));
	b->d = d;
	b->c = c;

	return b;
}

/*
 * Create a new block from the last list of descriptors
 * and the last command.
 */
void
block(void)
{
	struct block *b = new_block(last_d, last_c);
	last_d = NULL;
	last_c = NULL;
	list_add(&block_list, b);
}

/*
 * Free block.
 */
void
block_free(struct block *b)
{
	/* list of descriptors */
	struct list *desc_list = b->d;
	struct list *node;

	free(b->c);
	for (node = desc_list; node != NULL; node = node->next) {
		struct descriptor *desc = node->n;
		descriptor_free(desc);
	}
	free(desc_list);
	free(b);
}

/*
 * Return empty win_props structure.
 */
struct win_props *
new_win_props(void)
{
	struct win_props *p = malloc(sizeof(struct win_props));
	p->name = p->role = p->instance =
		p->class = p->type = NULL;

	return p;
}

/*
 * Free strings of win_props.
 */
void
free_win_props(struct win_props *p)
{
	free(p->class);
	free(p->instance);
	free(p->type);
	free(p->name);
	free(p->role);
	free(p);
}

/*
 * For debugging. Print window properties.
 */
void
print_win_props(struct win_props *p)
{
	fprintf(stderr, "name: \"%s\"\tclass: \"%s\"\tinstance: \"%s\"\ttype: \"%s\"\trole: \"%s\"\n", p->name,
			p->class, p->instance, p->type, p->role);
}

/*
 * Init ewmh connection.
 */
void
init_ewmh(void)
{
	ewmh = malloc(sizeof(xcb_ewmh_connection_t));
	if (!ewmh)
		warnx("couldn't set up ewmh connection");
	xcb_ewmh_init_atoms_replies(ewmh,
			xcb_ewmh_init_atoms(conn, ewmh), NULL);
}

/*
 * Get atom by name.
 */
xcb_atom_t
get_atom(const char *name)
{
	xcb_intern_atom_reply_t *r =
		xcb_intern_atom_reply(conn,
			xcb_intern_atom(conn, 0, strlen(name), name),
			NULL);
	xcb_atom_t atom;

	if (!r) {
		warnx("couldn't get atom '%s'\n", name);
		return XCB_ATOM_STRING;
	}
	atom = r->atom;
	free(r);

	return atom;
}

/*
 * Convert window type atom to string form.
 */
char *
window_type_to_string(xcb_ewmh_get_atoms_reply_t *reply)
{
	int i;
	char *atom_name = NULL;
	char *str;

	if (reply != NULL) {
		str = malloc((WINDOW_TYPE_STRING_LENGTH + 1) * sizeof(char));
		str[0] = '\0';
		for (i = 0; i < reply->atoms_len; i++) {
			xcb_atom_t a = reply->atoms[i];
#define WT_STRING(type, s) if (a == ewmh->_NET_WM_WINDOW_TYPE_##type) atom_name = strdup(#s);
			WT_STRING(DESKTOP, desktop)
			WT_STRING(DOCK, dock)
			WT_STRING(TOOLBAR, toolbar)
			WT_STRING(MENU, menu)
			WT_STRING(UTILITY, utility)
			WT_STRING(SPLASH, splash)
			WT_STRING(DIALOG, dialog)
			WT_STRING(DROPDOWN_MENU, dropdown_menu)
			WT_STRING(POPUP_MENU, popup_menu)
			WT_STRING(TOOLTIP, tooltip)
			WT_STRING(NOTIFICATION, notification)
			WT_STRING(COMBO, combo)
			WT_STRING(DND, dnd)
			WT_STRING(NORMAL, normal)
#undef WT_STRING

			if (atom_name != NULL) {
				if (i == 0)
					sprintf(str, "%s", atom_name);
				else
					sprintf(str, "%s,%s", str, atom_name);
			}

			free(atom_name);
			atom_name = NULL;
		}
	} else {
		str = strdup("");
	}

	return str;
}

/*
 * Get string property of window by atom.
 */
char *
get_string_prop(xcb_window_t win, xcb_atom_t prop, int utf8)
{
	char *p, *value;
	int len = 0;
	xcb_get_property_cookie_t c;
	xcb_get_property_reply_t *r = NULL;
	xcb_atom_t type;

	if (utf8)
		type = ewmh->UTF8_STRING;
	else
		type = XCB_ATOM_STRING;

	c = xcb_get_property(conn, 0, win,
			prop, type, 0L, 4294967295L);
	r = xcb_get_property_reply(conn, c, NULL);

	if (r == NULL || xcb_get_property_value_length(r) == 0) {
		p = strdup("");
		warnx("unable to get window property for 0x%08x", win);
	} else {
		len = xcb_get_property_value_length(r);
		p = malloc((len + 1) * sizeof(char));
		value = xcb_get_property_value(r);
		strncpy(p, value, len);
		p[len] = '\0';
		if (utf8)
			fprintf(stderr, "got utf8 prop\n");
	}
	free(r);

	return p;
}

struct win_props *
get_props(xcb_window_t win)
{
	struct win_props *p = new_win_props();
	int status;
	xcb_get_property_cookie_t c_class, c_type;
	xcb_icccm_get_wm_class_reply_t *r_class;
	xcb_ewmh_get_atoms_reply_t *r_type;

	/* WM_CLASS */
	c_class = xcb_icccm_get_wm_class(conn, win);
	r_class = malloc(sizeof(xcb_icccm_get_wm_class_reply_t));
	status = xcb_icccm_get_wm_class_reply(conn, c_class, r_class, NULL);
	if (status == 1) {
		p->class = strdup(r_class->class_name);
		p->instance = strdup(r_class->instance_name);
	} else {
		p->class = strdup("");
		p->instance = strdup("");
	}
	free(r_class);

	/* _NET_WM_WINDOW_TYPE */
	c_type = xcb_ewmh_get_wm_window_type(ewmh, win);
	r_type = malloc(sizeof(xcb_ewmh_get_atoms_reply_t));
	status = xcb_ewmh_get_wm_window_type_reply(ewmh, c_type, r_type, NULL);
	if (status == 1) {
		p->type = window_type_to_string(r_type);
	} else {
		p->type = strdup("");
	}
	free(r_type);

	/* WM_NAME */
	p->name = get_string_prop(win, ewmh->_NET_WM_NAME, 1);
	if (p->name[0] == '\0') {
		free(p->name);
		p->name = get_string_prop(win, XCB_ATOM_WM_NAME, 0);
	}

	/* WM_WINDOW_ROLE */
	p->role = get_string_prop(win, get_atom("WM_WINDOW_ROLE"), 0);

	return p;
}

/*
 * Match window props with descriptor_list.
 *
 * `l` is a list of descriptor structures.
 */
int
match_props(struct win_props *p, struct list *l)
{
	struct list *node;
	int status;
	char *to_match;
	int matched;

	node = l;
	status = 0;
	matched = 0;
	do {
		struct descriptor *d = node->n;
		switch (d->criterion) {
			case CRIT_CLASS: to_match = p->class; break;
			case CRIT_INSTANCE: to_match = p->instance; break;
			case CRIT_TYPE: to_match = p->type; break;
			case CRIT_NAME: to_match = p->name; break;
			case CRIT_ROLE: to_match = p->role; break;
			default: warnx("this is a bug, report it ASAP. (%s: line %d)", __FILE__, __LINE__); to_match = "";
		}

		/* avoid crash if regex failed to compile */
		if (d->reg != NULL)
			status = regexec(d->reg, to_match, 0, NULL, 0);
		else
			status = 1;
		printf("match %s (%s): %d\n", to_match, criterion_to_string(d->criterion), status);
		matched += (status == 0) * 1;

		node = node->next;
	} while (node != NULL && status == 0);

	return (node == NULL) * matched;
}

/*
 * Find matches of win_props with descriptor_lists in a list of blocks.
 *
 * That is, find the block that matches best with a window.
 */
struct block *
find_matching_block(struct win_props *p, struct list *l)
{
	struct list *node;
	int max_descriptors, descs;
	struct block *block = NULL;

	max_descriptors = 0;
	for (node = l; node != NULL; node = node->next) {
		struct block *b = node->n;
		struct list *desc_list = b->d;
		fprintf(stderr, "trying new block\n");
		descs = match_props(p, desc_list);
		if (descs > max_descriptors) {
			max_descriptors = descs;
			block = b;
			fprintf(stderr, "new maximum\n");
		}
	}

	if (block != NULL)
		fprintf(stderr, "will execute: `%s`\n", block->c);

	return block;
}

/*
 * Register events on existing windows.
 */
void
register_events(void)
{
	xcb_window_t *windows;
	int len, i;

	len = wm_get_windows(scrn->root, &windows);
	for (i = 0; i < len; i++) {
		if (wm_is_listable(windows[i], 0))
			wm_reg_event(windows[i], XCB_EVENT_MASK_PROPERTY_CHANGE);
	}
	free(windows);
}

/*
 * Handle X events.
 */
void
handle_events(void)
{
	xcb_generic_event_t *ev;
	xcb_window_t win;
	struct win_props *p;

	/* to receive window creation notifications */
	wm_reg_event(scrn->root, XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY);
	xcb_flush(conn);

	for (;;) {
		ev = xcb_wait_for_event(conn);
		if ((ev->response_type & ~0x80) == XCB_MAP_NOTIFY) {
			xcb_map_notify_event_t *ec = (xcb_map_notify_event_t *)ev;
			if (wm_is_listable(ec->window, 0)) {
				p = get_props(ec->window);
				print_win_props(p);
				find_matching_block(p, block_list);
				free_win_props(p);
				/* we need to get notified for further property changes */
				wm_reg_event(ec->window, XCB_EVENT_MASK_PROPERTY_CHANGE);
			}
		} else if ((ev->response_type & ~0x80) == XCB_PROPERTY_NOTIFY) {
			xcb_property_notify_event_t *en = (xcb_property_notify_event_t *)ev;
			if (wm_is_listable(en->window, 0)) {
				p = get_props(en->window);
				print_win_props(p);
				find_matching_block(p, block_list);
				free_win_props(p);
			}
		}
	}

	free(ev);
}

void
cleanup(void)
{
	struct list *l;
	for (l = block_list; l != NULL; l = l->next) {
		struct block *b = l->n;
		block_free(b);
	}
}

int
main(int argc, char **argv)
{
	if (argc == 1)
		print_usage(argv[0]);
	yyin = fopen(argv[1], "r");
	if (yyin == NULL)
		err(1, "couldn't open file");
	yyparse();

	if (wm_init_xcb() == 0)
		errx(1, "error while estabilishing connection to the X server");
	if (wm_get_screen() == 0)
		errx(1, "couldn't get X screen");
	init_ewmh();

	struct list *l;
	for (l = block_list; l != NULL; l = l->next) {
		struct block *b = l->n;
		struct list *ld;
		for (ld = b->d; ld != NULL; ld = ld->next) {
			struct descriptor *d = ld->n;
			char *c = criterion_to_string(d->criterion);
			printf("%s = \"%s\" ", c, d->str);
			free(c);
		}
		printf("\n`%s`\n", b->c);
	}

	register_events();
	handle_events();
	wm_kill_xcb();
	return 0;
}
