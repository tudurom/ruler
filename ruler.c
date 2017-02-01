#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ruler.h"

extern FILE * yyin;
struct list *last_d = NULL;
struct list *block_list = NULL;
command_t last_c;

void
print_usage(const char *program_name)
{
	fprintf(stderr, "Usage: %s <filename>\n", program_name);
	exit(1);
}

char *
strip_quotes(const char *str)
{
	/* length of original string - 2 (two quotes) + 1 (a null terminator) */
	int len = strlen(str) - 2;
	char *s = malloc((len + 1) * sizeof(char));
	int i;

	for (i = 1; i < strlen(str) - 1; i++) {
		s[i - 1] = str[i];
	}
	s[len] = '\0';

	return s;
}

struct descriptor *
new_descriptor(char *criterion, char *str)
{
	struct descriptor *d = malloc(sizeof(struct descriptor));
	d->criterion = criterion;
	d->str = str;

	return d;
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

int
main(int argc, char **argv)
{
	if (argc == 1)
		print_usage(argv[0]);
	yyin = fopen(argv[1], "r");
	if (yyin == NULL)
		err(1, "couldn't open file");
	yyparse();

	struct list *l;
	for (l = block_list; l != NULL; l = l->next) {
		struct block *b = l->n;
		struct list *ld;
		for (ld = b->d; ld != NULL; ld = ld->next) {
			struct descriptor *d = ld->n;
			printf("%s = %s ", d->criterion, d->str);
		}
		printf("\n`%s`\n", b->c);
	}
	return 0;
}
