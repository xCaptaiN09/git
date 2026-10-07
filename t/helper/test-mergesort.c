#include "test-tool.h"
#include "mem-pool.h"
#include "mergesort.h"
#include "strbuf.h"

struct line {
	char *text;
	struct line *next;
};

DEFINE_LIST_SORT(static, sort_lines, struct line, next);

static int compare_strings(const struct line *x, const struct line *y)
{
	return strcmp(x->text, y->text);
}

static int sort_stdin(void)
{
	struct line *lines;
	struct line **tail = &lines;
	struct strbuf sb = STRBUF_INIT;
	struct mem_pool lines_pool;
	char *p;

	strbuf_read(&sb, 0, 0);

	/*
	 * Split by newline, but don't create an item
	 * for the empty string after the last separator.
	 */
	if (sb.len && sb.buf[sb.len - 1] == '\n')
		strbuf_setlen(&sb, sb.len - 1);

	mem_pool_init(&lines_pool, 0);
	p = sb.buf;
	for (;;) {
		char *eol = strchr(p, '\n');
		struct line *line = mem_pool_alloc(&lines_pool, sizeof(*line));
		line->text = p;
		*tail = line;
		tail = &line->next;
		if (!eol)
			break;
		*eol = '\0';
		p = eol + 1;
	}
	*tail = NULL;

	sort_lines(&lines, compare_strings);

	while (lines) {
		puts(lines->text);
		lines = lines->next;
	}
	mem_pool_discard(&lines_pool, 0);
	strbuf_release(&sb);
	return 0;
}

int cmd__mergesort(int argc, const char **argv)
{
	if (argc == 2 && !strcmp(argv[1], "sort"))
		return sort_stdin();
	fprintf(stderr, "usage: test-tool mergesort sort\n");
	return 129;
}
