#include "unit-test.h"
#include "mergesort.h"

static uint32_t minstd_rand(uint32_t *state)
{
	*state = (uint64_t)*state * 48271 % 2147483647;
	return *state;
}

static void dist_sawtooth(int *arr, int n, int m)
{
	int i;
	for (i = 0; i < n; i++)
		arr[i] = i % m;
}

static void dist_rand(int *arr, int n, int m)
{
	int i;
	uint32_t seed = 1;
	for (i = 0; i < n; i++)
		arr[i] = minstd_rand(&seed) % m;
}

static void dist_stagger(int *arr, int n, int m)
{
	int i;
	for (i = 0; i < n; i++)
		arr[i] = (i * m + i) % n;
}

static void dist_plateau(int *arr, int n, int m)
{
	int i;
	for (i = 0; i < n; i++)
		arr[i] = (i < m) ? i : m;
}

static void dist_shuffle(int *arr, int n, int m)
{
	int i, j, k;
	uint32_t seed = 1;
	for (i = j = 0, k = 1; i < n; i++)
		arr[i] = minstd_rand(&seed) % m ? (j += 2) : (k += 2);
}

#define DIST(name) { #name, dist_##name }

static struct dist {
	const char *name;
	void (*fn)(int *arr, int n, int m);
} dist[] = {
	DIST(sawtooth),
	DIST(rand),
	DIST(stagger),
	DIST(plateau),
	DIST(shuffle),
};

static void mode_copy(int *arr UNUSED, int n UNUSED)
{
	/* nothing */
}

static void mode_reverse(int *arr, int n)
{
	int i, j;
	for (i = 0, j = n - 1; i < j; i++, j--)
		SWAP(arr[i], arr[j]);
}

static void mode_reverse_1st_half(int *arr, int n)
{
	mode_reverse(arr, n / 2);
}

static void mode_reverse_2nd_half(int *arr, int n)
{
	int half = n / 2;
	mode_reverse(arr + half, n - half);
}

static int compare_ints(const void *av, const void *bv)
{
	const int *ap = av, *bp = bv;
	int a = *ap, b = *bp;
	return (a > b) - (a < b);
}

static void mode_sort(int *arr, int n)
{
	QSORT(arr, n, compare_ints);
}

static void mode_dither(int *arr, int n)
{
	int i;
	for (i = 0; i < n; i++)
		arr[i] += i % 5;
}

static void unriffle(int *arr, int n, int *tmp)
{
	int i, j;
	COPY_ARRAY(tmp, arr, n);
	for (i = j = 0; i < n; i += 2)
		arr[j++] = tmp[i];
	for (i = 1; i < n; i += 2)
		arr[j++] = tmp[i];
}

static void unriffle_recursively(int *arr, int n, int *tmp)
{
	if (n > 1) {
		int half = n / 2;
		unriffle(arr, n, tmp);
		unriffle_recursively(arr, half, tmp);
		unriffle_recursively(arr + half, n - half, tmp);
	}
}

static void mode_unriffle(int *arr, int n)
{
	int *tmp;
	ALLOC_ARRAY(tmp, n);
	unriffle_recursively(arr, n, tmp);
	free(tmp);
}

static unsigned int prev_pow2(unsigned int n)
{
	unsigned int pow2 = 1;
	while (pow2 * 2 < n)
		pow2 *= 2;
	return pow2;
}

static void unriffle_recursively_skewed(int *arr, int n, int *tmp)
{
	if (n > 1) {
		int pow2 = prev_pow2(n);
		int rest = n - pow2;
		unriffle(arr + pow2 - rest, rest * 2, tmp);
		unriffle_recursively_skewed(arr, pow2, tmp);
		unriffle_recursively_skewed(arr + pow2, rest, tmp);
	}
}

static void mode_unriffle_skewed(int *arr, int n)
{
	int *tmp;
	ALLOC_ARRAY(tmp, n);
	unriffle_recursively_skewed(arr, n, tmp);
	free(tmp);
}

#define MODE(name) { #name, mode_##name }

static struct mode {
	const char *name;
	void (*fn)(int *arr, int n);
} mode[] = {
	MODE(copy),
	MODE(reverse),
	MODE(reverse_1st_half),
	MODE(reverse_2nd_half),
	MODE(sort),
	MODE(dither),
	MODE(unriffle),
	MODE(unriffle_skewed),
};

struct number {
	int value, rank;
	struct number *next;
};

DEFINE_LIST_SORT_DEBUG(static, sort_numbers, struct number, next,
		       (void)0, (void)0);

static int compare_numbers(const struct number *an, const struct number *bn)
{
	int a = an->value, b = bn->value;
	return (a > b) - (a < b);
}

/* Free the storage directly, even if an assertion fails on a broken list. */
static int *values;
static struct number *numbers;

void test_mergesort__cleanup(void)
{
	FREE_AND_NULL(values);
	FREE_AND_NULL(numbers);
}

static struct number *prepare_list(const int *arr, int n)
{
	int i;

	ALLOC_ARRAY(numbers, n);
	for (i = 0; i < n; i++) {
		numbers[i].value = arr[i];
		numbers[i].rank = i;
		numbers[i].next = i + 1 < n ? &numbers[i + 1] : NULL;
	}
	return n ? numbers : NULL;
}

static void check_list(struct number *list, const int *expected,
		       int n, const char *context)
{
	struct number *previous = NULL;
	int i;

	/* Bound traversal so a cycle is reported as an overlong list. */
	for (i = 0; i < n; i++) {
		cl_assert_(list, context);
		cl_assert_equal_i_(list->value, expected[i], "%s: index %d",
				   context, i);
		if (previous && previous->value == list->value)
			cl_assert_lt_i_(previous->rank, list->rank,
					"%s: stability at index %d", context, i);
		previous = list;
		list = list->next;
	}
	cl_assert_(list == NULL, context);
}

/*
 * A version of the qsort certification program from "Engineering a Sort
 * Function" by Bentley and McIlroy, Software—Practice and Experience,
 * Volume 23, Issue 11, 1249–1265 (November 1993).
 */
static void certify(const struct dist *distribution)
{
	static const int sizes[] = { 100, 1023, 1024, 1025 };
	size_t i, j;
	int m;

	for (i = 0; i < ARRAY_SIZE(sizes); i++) {
		int n = sizes[i];

		for (j = 0; j < ARRAY_SIZE(mode); j++) {
			for (m = 1; m < 2 * n; m *= 2) {
				struct number *list;
				char context[128];

				xsnprintf(context, sizeof(context),
					  "%s %s n=%d m=%d",
					  distribution->name, mode[j].name, n, m);
				ALLOC_ARRAY(values, n);
				distribution->fn(values, n, m);
				mode[j].fn(values, n);
				list = prepare_list(values, n);
				sort_numbers(&list, compare_numbers);
				QSORT(values, n, compare_ints);
				check_list(list, values, n, context);
				test_mergesort__cleanup();
			}
		}
	}
}

void test_mergesort__sawtooth(void)
{
	certify(&dist[0]);
}

void test_mergesort__rand(void)
{
	certify(&dist[1]);
}

void test_mergesort__stagger(void)
{
	certify(&dist[2]);
}

void test_mergesort__plateau(void)
{
	certify(&dist[3]);
}

void test_mergesort__shuffle(void)
{
	certify(&dist[4]);
}
