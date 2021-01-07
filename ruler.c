#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>
#include <unistd.h>

#include <sys/wait.h>
#include <sys/select.h>
#include <xcb/xcb.h>
#include <xcb/randr.h>
#include <xcb/xcb_icccm.h>
#include <xcb/xcb_ewmh.h>
#include <wm.h>

#include "arg.h"
#include "asprintf.h"
#include "ruler.h"

extern FILE * yyin;

struct list *last_d = NULL;
struct list *block_list = NULL;
struct list *win_list = NULL;

command_t last_c;
extern char **environ;

const int _debug = DEBUG;
struct conf conf;

int state_run = 0, state_reload = 0, state_pause = 0;

char *argv0;
char **configs;
int no_of_configs;

xcb_connection_t *conn;
xcb_screen_t *scrn;
xcb_ewmh_connection_t *ewmh;

xcb_atom_t allowed_atoms[NR_ATOMS];

void
print_usage(const char *program_name, int exit_value)
{
	fprintf(stderr, "Usage: %s [-himopv] [-s shell] filename [filename...]\n", program_name);
	exit(exit_value);
}

void print_version(void)
{
	fprintf(stderr, "%s %s\n", NAME, VERSION);
	fprintf(stderr, "Copyright (c) 2017 Tudor Ioan Roman\n");
	fprintf(stderr, "Released under the ISC License\n");
	exit(0);
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

	DMSG("new regex from `%s`\n", str);
	status = regcomp(d->reg, str, REGEX_FLAGS | (conf.case_insensitive * REG_ICASE));
	if (status != 0) {
		warnx("couldn't compile regex for %s=\"%s\". Check your regex.", criterion, str);
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
 * Find node and delete it if it exists.
 */
void
list_delete(struct list **list, struct list *item)
{
	struct list *prev;

	if (*list == NULL || item == NULL)
		return;

	prev = *list;
	while (prev != NULL && prev->next != item)
		prev = prev->next;

	if (prev != NULL)
		prev->next = item->next;
	if (item == *list)
		*list = item->next;
	free(item->n);
	free(item);
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
	DMSG("name: \"%s\"\tclass: \"%s\"\tinstance: \"%s\"\ttype: \"%s\"\trole: \"%s\"\n", p->name,
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
 * Populate the list of allowed atoms.
 */
void
populate_allowed_atoms(void)
{
	int i;

	for (i = 0; i < NR_ATOMS; i++) {
		allowed_atoms[i] = get_atom(atom_names[i]);
	}
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
		DMSG("unable to get window property for 0x%08x\n", win);
	} else {
		len = xcb_get_property_value_length(r);
		p = malloc((len + 1) * sizeof(char));
		value = xcb_get_property_value(r);
		strncpy(p, value, len);
		p[len] = '\0';
	}
	free(r);

	return p;
}

/*
 * Fill win_props structure.
 */
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
		p->name = get_string_prop(win, allowed_atoms[ATOM_WM_NAME], 0);
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
		DMSG("match \"%s\" (%s): %d\n", to_match, criterion_to_string(d->criterion), status);
		matched += (status == 0) * 1;

		node = node->next;
	} while (node != NULL && status == 0);

	return (node == NULL) * matched;
}

/*
 * Find matching blocks for a window and put them in the `blocks` list.
 */
void
find_matching_blocks(struct win_props *p, struct list *l, struct list **blocks)
{
	struct list *node;
	int m;

	if (*blocks != NULL)
		list_free(blocks);

	for (node = l; node != NULL; node = node->next) {
		struct block *b = node->n;
		struct list *desc_list = b->d;
		DMSG("trying new block\n");
		m = match_props(p, desc_list);
		if (m > 0) {
			list_add(blocks, b);
		}
	}
}

/*
 * Execute program with arguments.
 *
 * cmd is an array of strings representing the
 * executable and the arguments. The last element
 * of cmd must be NULL.
 *
 * This function should be called in a separate process,
 * otherwise the execution of the caller process ends.
 *
 * See execvp(3).
 */
void
execute(char **cmd)
{
	setsid();
	execvp(cmd[0], cmd);
	err(1, "command execution failed");
}

/*
 * Pipe command to shell.
 *
 * Creates two subprocesses.
 * The first prints to stdout the command.
 * The second is the shell that interprets the command.
 */
void
spawn(char *shell, command_t cmd)
{
	int desc[2];
	if (pipe(desc) == -1) {
		warnx("pipe failed");
		return;
	}

	if (fork() == 0) {
		close(STDOUT_FILENO);
		dup(desc[1]);
		close(desc[0]);
		close(desc[1]);

		fputs(cmd, stdout);
		exit(0);
	}

	if (fork() == 0) {
		close(STDIN_FILENO);
		dup(desc[0]);
		close(desc[1]);
		close(desc[0]);

		char *toexec[] = { shell, NULL };
		execute(toexec);
	}

	close(desc[0]);
	close(desc[1]);
	wait(NULL);
	wait(NULL);
}

/*
 * Run command in the given shell.
 */
void run_command(char *shell, command_t cmd, int sync)
{
	DMSG("will execute: `%s`\n", cmd);

	if (sync) {
		spawn(shell, cmd);
	} else {
		if (fork() == 0) {
			if (conn != NULL)
				close(xcb_get_file_descriptor(conn));
			spawn(shell, cmd);
			exit(0);
		}
	}
}

/*
 * Find matching block for a window and execute the command.
 */
void
execute_matching_block(struct win_props *props, struct list *blocks)
{
	struct list *matching_blocks = NULL, *node;
	struct block *b;
	char chr;
	int i, skip, sync;

	find_matching_blocks(props, blocks, &matching_blocks);
	if (matching_blocks != NULL) {
		for (node = matching_blocks; node != NULL; node = node->next) {
			b = node->n;
			i = 0;
			while ((chr = b->c[i]) != '\0' && isblank(chr))
				i++;

			if (chr == '\0') {
				warnx("either the supplied file is strange "
						"or this is a bug and you should report it ASAP "
						"(%s: line %d", __FILE__, __LINE__);
				return;
			}

			sync = 0;
			skip = i;
			if (chr == ';') {
				sync = 1;
				skip++;
			}
			run_command(conf.shell, b->c + skip, sync);
		}
		list_free(&matching_blocks);
	}
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
			wm_reg_window_event(windows[i], XCB_EVENT_MASK_PROPERTY_CHANGE);
	}
	free(windows);
}

void
set_environ(xcb_window_t win)
{
	char *wid = malloc((2 + 8 + 1) * sizeof(char));
	sprintf(wid, "0x%08x", win);
	setenv(ENV_VARIABLE, wid, 1);
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
	int xcb_desc = xcb_get_file_descriptor(conn);
	fd_set descs;

	/* to receive window creation notifications */
	wm_reg_window_event(scrn->root, XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY);
	xcb_flush(conn);

	state_run = 1;
	state_reload = 0;
	state_pause = 0;
	while (state_run) {
		FD_ZERO(&descs);
		FD_SET(xcb_desc, &descs);

		/*
		 * We can't use xcb_wait_for_event because that means
		 * that after receiving a signal to exit, the program will wait
		 * for an X event and then it will exit.
		 *
		 * Instead, we are checking if there are events and then handle them.
		 * select should just fail if we abort the execution of the program,
		 * thus restarting the loop and then exiting from it because state_run
		 * will be 0.
		 */
		if (select(xcb_desc + 1, &descs, NULL, NULL, NULL) > 0) {
			while((ev = xcb_poll_for_event(conn)) != NULL) {
				win = -1;

				/* do work only if not paused */
				if (state_pause == 0) {
					if ((ev->response_type & ~0x80) == XCB_MAP_NOTIFY) {
						xcb_map_notify_event_t *ec = (xcb_map_notify_event_t *)ev;

						if ((conf.catch_override_redirect || wm_is_listable(ec->window, 0))
								&& (conf.exec_on_map || is_new_window(ec->window))) {
							win = ec->window;
							DMSG("new window created: 0x%08x\n", win);

							/* we need to get notified for further property changes */
							if (conf.exec_on_prop_change) {
								wm_reg_window_event(ec->window, XCB_EVENT_MASK_PROPERTY_CHANGE);
							}
						}
					} else if (conf.exec_on_prop_change && (ev->response_type & ~0x80) == XCB_PROPERTY_NOTIFY) {
						xcb_property_notify_event_t *en = (xcb_property_notify_event_t *)ev;
						int pos = 0;
						while (pos < NR_ATOMS && allowed_atoms[pos] != en->atom)
							pos++;

						if (pos < NR_ATOMS && (conf.catch_override_redirect || wm_is_listable(en->window, 0)))
							win = en->window;
					} else if ((ev->response_type & ~0x80) == XCB_DESTROY_NOTIFY) {
						xcb_destroy_notify_event_t *ed = (xcb_destroy_notify_event_t *)ev;
						struct list *l = win_list;

						win = ed->window;
						while (l != NULL && *(xcb_window_t *)l->n != win)
							l = l->next;

						if (l != NULL) {
							list_delete(&win_list, l);
							DMSG("removed window 0x%08x from list\n", win);
						}
						win = -1;
					}

					/* do the actual work. get props, find matches, execute commands */
					if (win != -1 && state_pause == 0) {
						p = get_props(win);
						print_win_props(p);
						set_environ(win);
						execute_matching_block(p, block_list);
						free_win_props(p);
					}
				}

				free(ev);
				ev = NULL;
			}
		}

		if (state_reload) {
			reload_config();
			state_reload = 0;
		}

		if (xcb_connection_has_error(conn)) {
			warnx("X server errored");
			state_run = 0;
		}

	}
}

/*
 * Returns 1 if the windows has been created but not destroyed.
 * 0 otherwise.
 */
int
is_new_window(xcb_window_t win)
{
	struct list *l;
	xcb_window_t *w;

	l = win_list;
	while (l != NULL && *(w = l->n) != win)
		l = l->next;

	if (l == NULL) {
		w = malloc(sizeof(xcb_window_t));
		*w = win;
		list_add(&win_list, w);

		return 1;
	} else {
		return 0;
	}
}

void
cleanup(void)
{
	struct list *l;
	for (l = block_list; l != NULL; l = l->next) {
		struct block *b = l->n;
		block_free(b);
	}
	list_free(&block_list);

	for (l = last_d; l != NULL; l = l->next) {
		struct descriptor *d = l->n;
		descriptor_free(d);
	}
	list_free(&last_d);
}

void
init_conf(void)
{
	conf.case_insensitive        = 0;
	conf.shell                   = getenv("SHELL");
	conf.catch_override_redirect = 0;
	conf.exec_on_prop_change     = 0;
	conf.exec_on_map             = 0;
}

/*
 * Signal handler.
 */
void
handle_sig(int sig)
{
	switch (sig) {
		case SIGHUP:
		case SIGINT:
		case SIGTERM:
			state_run = 0;
			break;
		case SIGUSR1:
			state_reload = 1;
			break;
		case SIGUSR2:
			state_pause = !state_pause;
			break;
	}
}

/*
 * Returns 0 if parsing succeeded.
 */
int
parse_file(char *fp)
{
	yyin = fopen(fp, "r");
	if (yyin == NULL)
		return 1;
	yyparse();
	fclose(yyin);
	yyrestart(yyin);

	return 0;
}

void
reload_config(void)
{
	int i;
	char *xdg_home = getenv("XDG_CONFIG_HOME");
	char *xdg_cfg_path;

	cleanup();
	if (xdg_home == NULL)
		asprintf(&xdg_home, "%s/.config", getenv("HOME"));

	asprintf(&xdg_cfg_path, "%s/ruler/rulerrc", xdg_home);
	if (parse_file(xdg_cfg_path) == 1 && no_of_configs == 0)
		errx(1, "couldn't open config file '%s' (%s). No other config files supplied, exiting", xdg_cfg_path, strerror(errno));
	free(xdg_cfg_path);
	free(xdg_home);

	for (i = 0; i < no_of_configs; i++) {
		if (parse_file(configs[i]) != 0)
			err(1, "couldn't open config file '%s'", configs[i]);
	}

	DMSG("configs reloaded\n");
}

int
main(int argc, char **argv)
{
	init_conf();

	/* see arg.h */
	ARGBEGIN {
		case 'i':
			conf.case_insensitive = 1; break;
		case 's':
			conf.shell = EARGF((
						warnx("option 's' requires an argument"),
						print_usage(argv0, 1)
					)); break;
		case 'o':
			conf.catch_override_redirect = 1; break;
		case 'p':
			conf.exec_on_prop_change = 1; break;
		case 'm':
			conf.exec_on_map = 1; break;
		case 'h':
			print_usage(argv0, 0); break;
		case 'v':
			print_version(); break;
	} ARGEND

	/* the remaining arguments should be files */
	no_of_configs = argc;
	DMSG("%d extra config files\n", no_of_configs);
	configs = argv;
	reload_config();

	if (_debug) {
		struct list *l;
		for (l = block_list; l != NULL; l = l->next) {
			struct block *b = l->n;
			struct list *ld;
			for (ld = b->d; ld != NULL; ld = ld->next) {
				struct descriptor *d = ld->n;
				char *c = criterion_to_string(d->criterion);
				DMSG("%s = \"%s\" ", c, d->str);
				free(c);
			}
			DMSG("\n`%s`\n", b->c);
		}
	}

	if (wm_init_xcb() == -1)
		errx(1, "error while estabilishing connection to the X server");
	if (wm_get_screen() == -1)
		errx(1, "couldn't get X screen");
	init_ewmh();

	/* don't let childrens become zombies. kill them for real (bwahaha) */
	signal(SIGCHLD, SIG_IGN);
	/* more signals */
	signal(SIGINT, handle_sig);
	signal(SIGHUP, handle_sig);
	signal(SIGTERM, handle_sig);
	signal(SIGUSR1, handle_sig);
	signal(SIGUSR2, handle_sig);

	populate_allowed_atoms();
	register_events();
	handle_events();
	wm_kill_xcb();
	return 0;
}
