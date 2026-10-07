#!/bin/sh

test_description='verify sort functions'

. ./test-lib.sh

test_expect_success 'DEFINE_LIST_SORT_DEBUG' '
	test-tool mergesort test
'

test_expect_success 'sort stdin' '
	printf "%s\n" c a b >input &&
	printf "%s\n" a b c >expect &&
	test-tool mergesort sort <input >actual &&
	test_cmp expect actual
'

test_done
