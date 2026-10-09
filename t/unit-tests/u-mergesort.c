#include "unit-test.h"
#include "mergesort.h"

struct number {
	int value;
	size_t rank;
	struct number *next;
};

DEFINE_LIST_SORT(static, sort_numbers, struct number, next);

static int get_next_count, set_next_count;

DEFINE_LIST_SORT_DEBUG(static, sort_numbers_debug, struct number, next,
		       get_next_count++, set_next_count++);

static int compare_numbers(const struct number *a, const struct number *b)
{
	return (a->value > b->value) - (a->value < b->value);
}

static int compare_ints(const void *va, const void *vb)
{
	const int *a = va, *b = vb;

	return (*a > *b) - (*a < *b);
}

static struct number *numbers;
static int *expected;

void test_mergesort__cleanup(void)
{
	FREE_AND_NULL(numbers);
	FREE_AND_NULL(expected);
}

static void check_sort(const int *input, size_t nr)
{
	struct number *list, *previous = NULL;

	ALLOC_ARRAY(numbers, nr);
	ALLOC_ARRAY(expected, nr);
	COPY_ARRAY(expected, input, nr);
	QSORT(expected, nr, compare_ints);
	for (size_t i = 0; i < nr; i++) {
		numbers[i].value = input[i];
		numbers[i].rank = i;
		numbers[i].next = i + 1 < nr ? &numbers[i + 1] : NULL;
	}

	list = nr ? numbers : NULL;
	sort_numbers(&list, compare_numbers);
	for (size_t i = 0; i < nr; i++) {
		cl_assert_(list, "list is too short");
		cl_assert_equal_i_(list->value, expected[i],
				   "size %zu, item %zu", nr, i);
		if (previous && previous->value == list->value)
			cl_assert_lt_i_(previous->rank, list->rank,
					"stability: size %zu, item %zu", nr, i);
		previous = list;
		list = list->next;
	}
	cl_assert_(list == NULL, "list is too long");
	test_mergesort__cleanup();
}

static const size_t sizes[] = { 100, 1023, 1024, 1025 };

void test_mergesort__sorted(void)
{
	int input[1025];

	for (size_t i = 0; i < ARRAY_SIZE(input); i++)
		input[i] = i;
	for (size_t i = 0; i < ARRAY_SIZE(sizes); i++)
		check_sort(input, sizes[i]);
}

void test_mergesort__reversed(void)
{
	int input[1025];

	for (size_t i = 0; i < ARRAY_SIZE(sizes); i++) {
		for (size_t j = 0; j < sizes[i]; j++)
			input[j] = sizes[i] - j;
		check_sort(input, sizes[i]);
	}
}

void test_mergesort__equal_values(void)
{
	int input[1025] = { 0 };

	for (size_t i = 0; i < ARRAY_SIZE(sizes); i++)
		check_sort(input, sizes[i]);
}

void test_mergesort__random(void)
{
	int input[1025];
	uint32_t seed = 1;

	for (size_t i = 0; i < ARRAY_SIZE(input); i++) {
		seed = (uint64_t)seed * 48271 % 2147483647;
		input[i] = seed % 32;
	}
	for (size_t i = 0; i < ARRAY_SIZE(sizes); i++)
		check_sort(input, sizes[i]);
}

void test_mergesort__empty(void)
{
	check_sort(NULL, 0);
}

void test_mergesort__singleton(void)
{
	const int input[] = { 42 };

	check_sort(input, ARRAY_SIZE(input));
}

void test_mergesort__reversed_pair(void)
{
	const int input[] = { 2, 1 };

	check_sort(input, ARRAY_SIZE(input));
}

void test_mergesort__equal_pair(void)
{
	const int input[] = { 1, 1 };

	check_sort(input, ARRAY_SIZE(input));
}

void test_mergesort__interleaved_runs(void)
{
	const int input[] = { 0, 2, 4, 6, 1, 3, 5, 7 };

	check_sort(input, ARRAY_SIZE(input));
}

void test_mergesort__mixed_values(void)
{
	const int input[] = { INT_MAX, -1, 0, INT_MIN, -1, INT_MAX, 0 };

	check_sort(input, ARRAY_SIZE(input));
}

void test_mergesort__debug_hooks(void)
{
	struct number nodes[] = {
		{ .value = 2 },
		{ .value = 1 },
	};
	struct number *list = &nodes[0];

	nodes[0].next = &nodes[1];
	get_next_count = set_next_count = 0;
	sort_numbers_debug(&list, compare_numbers);
	cl_assert_equal_p(list, &nodes[1]);
	cl_assert_equal_p(list->next, &nodes[0]);
	cl_assert_equal_p(list->next->next, NULL);
	cl_assert_gt_i(get_next_count, 0);
	cl_assert_gt_i(set_next_count, 0);
}
