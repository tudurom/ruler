#ifndef __RULER_H
#define __RULER_H

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE
#endif

typedef char * command_t;

struct descriptor {
	char *criterion, *str;
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

void yyerror(const char *);
int yywrap(void);
int yylex(void);
int yyparse(void);

void print_usage(const char *);
char * strip_quotes(const char *);

struct descriptor * new_descriptor(char *, char *);
void desc(char *, char *);

void list_add(struct list **, void *node);
void list_free(struct list **);

command_t new_command(char *);
void comm(char *);

struct block * new_block(struct list *, command_t);
void block(void);

#endif
