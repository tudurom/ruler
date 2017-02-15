#ifndef __RULER_H
#define __RULER_H

#include <xcb/xcb_ewmh.h>
#include <regex.h>

#define WINDOW_TYPE_STRING_LENGTH 110
#define REGEX_FLAGS REG_EXTENDED | REG_NOSUB
#define ENV_VARIABLE "RULER_WID"
#define DEBUG 0

#ifndef NAME
#define NAME "ruler"
#endif

#ifndef VERSION
#define VERSION "-1"
#endif

enum {
	ATOM_WM_NAME,
	ATOM_WM_CLASS,
	ATOM_WM_ROLE,
	ATOM_WM_TYPE,
	NR_ATOMS
};

static const char *atom_names[] = {
	"WM_NAME",
	"WM_CLASS",
	"WM_WINDOW_ROLE",
	"_NET_WM_WINDOW_TYPE"
};

#define DMSG(fmt, ...) if (_debug) { fprintf(stderr, fmt, ##__VA_ARGS__); }

typedef char * command_t;

enum criterion {
	CRIT_CLASS,
	CRIT_INSTANCE,
	CRIT_TYPE,
	CRIT_NAME,
	CRIT_ROLE
};

struct descriptor {
	enum criterion criterion;
	char *str;
	regex_t *reg;
};

struct list {
	void *n;
	struct list *next;
	struct list *prev;
};

struct block {
	/* list of descriptors */
	struct list *d;
	command_t c;
};

struct win_props {
	char *class;
	char *instance;
	char *type;
	char *name;
	char *role;
};

struct conf {
	int case_insensitive;
	char *shell;
	int catch_override_redirect;
	int exec_on_prop_change;
	int exec_on_map;
};

void yyerror(const char *);
int yywrap(void);
int yylex(void);
int yyparse(void);
void yyrestart(FILE *);

void print_usage(const char *, int);
void print_version(void);
char * strip_quotes(char *);

struct descriptor * new_descriptor(char *, char *);
void desc(char *, char *);
void descriptor_free(struct descriptor *);

void list_add(struct list **, void *node);
void list_remove(struct list **, struct list *);
void list_free(struct list **);

command_t new_command(char *);
void comm(char *);

struct block * new_block(struct list *, command_t);
void block(void);
void block_free(struct block *);

struct win_props * new_win_props(void);
void free_win_props(struct win_props *);
void print_win_props(struct win_props *);

void init_ewmh(void);
xcb_atom_t get_atom(const char *);
void populate_allowed_atoms(void);

char * window_type_to_string(xcb_ewmh_get_atoms_reply_t *);
char * get_string_prop(xcb_window_t, xcb_atom_t, int);

struct win_props * get_props(xcb_window_t);
int match_props(struct win_props *, struct list *);
void find_matching_blocks(struct win_props *, struct list *, struct list **);

void execute(char **);
void spawn(char *, command_t);
void run_command(char *shell, command_t, int);

void execute_matching_block(struct win_props *, struct list *);

void register_events(void);
void set_environ(xcb_window_t);
void handle_events(void);

int is_new_window(xcb_window_t);

void cleanup(void);

void init_conf(void);

void handle_sig(int);
int parse_file(char *);
void reload_config(void);

#endif
