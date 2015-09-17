#!/usr/bin/perl

use strict;
use warnings;

my @templates = qw{ vector queue deque stack list forward_list unordered_map map
	unique_lock lock_guard
	decay is_void is_same result_of enable_if remove_reference conditional
	integral_constant remove_cv is_base_of
	pair tuple get function
	shared_ptr unique_ptr
	tuple_element tuple_size
	make_index_sequence make_integer_sequence
	make_pair make_tuple make_shared make_unique
	make_signed
	reference_wrapper
	atomic
	forward forward_as_tuple
	index_sequence integer_sequence is_integral numeric_limits
};

my @terms = qw{
	condition_variable mutex
	exception logic_error runtime_error invalid_argument overflow_error
	thread string stringstream
	for_each reduce exception_ptr true_type false_type
	current_exception rethrow_exception
	make_pair make_tuple make_shared make_unique
	move forward_as_tuple move_if_noexcept swap
	atomic_flag memory_order_acquire memory_order_release this_thread
	try_lock defer_lock adopt_lock
};

my @chrono = qw{
	duration time_point steady_clock system_clock high_resolution_clock
};

my $s_templates = '(?<!std::)(?<!\.)\b(' . join('|', @templates) . ')(?=<)';
my $s_terms = '(?<!std::)(?<!\.)\b(' . join('|', @terms) . ')(?![<\w])';
my $s_chrono = '(?<!chrono::)\b(' . join('|', @chrono) . ')\b';

my $r_templates = qr{$s_templates};
my $r_terms = qr{$s_terms};
my $r_chrono = qr{$s_chrono};

my $last_blank = 0;

while (<>) {
	if (/^\s*using namespace/) {
		next;
	}
	if (/^#include/) {
		print;
		next;
	}
	if (/^\s*(\/\/|\/\*|\*|\*\/)/) {
		print;
		next;
	}
	s/$r_templates/std::$1/g;
	s/$r_terms/std::$1/g;
	s/$r_chrono/std::chrono::$1/g;
	my $blank = !length;
	if ($last_blank and $blank) {
		next;
	}
	$last_blank = $blank;
	print;
}
