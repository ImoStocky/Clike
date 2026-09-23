#!/bin/sh

set -u

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ifj_bin=${IFJ_BIN:-"$root/build/ifj"}
tmp=$(mktemp)
trap 'rm -f "$tmp"' EXIT HUP INT TERM

passed=0
failed=0

for source in "$root"/tests/*.src; do
	name=${source##*/}
	name=${name%.src}
	input="$root/tests/$name.in"
	expected="$root/tests/$name.out"
	expected_status_file="$root/tests/$name.status"
	expected_status=0

	if [ -f "$expected_status_file" ]; then
		expected_status=$(cat "$expected_status_file")
	fi

	if [ ! -f "$expected" ] && [ ! -f "$expected_status_file" ]; then
		echo "FAIL $name (missing expected output)"
		failed=$((failed + 1))
		continue
	fi

	if [ -f "$input" ]; then
		"$ifj_bin" "$source" < "$input" > "$tmp"
	else
		"$ifj_bin" "$source" < /dev/null > "$tmp"
	fi
	status=$?

	if [ -f "$expected" ]; then
		actual_output=$(cat "$tmp")
		expected_output=$(cat "$expected")
	else
		actual_output=$(cat "$tmp")
		expected_output=
	fi

	if [ "$status" -eq "$expected_status" ] &&
		[ "$actual_output" = "$expected_output" ]; then
		echo "PASS $name"
		passed=$((passed + 1))
	else
		echo "FAIL $name (exit $status, expected $expected_status)"
		if [ -f "$expected" ]; then
			diff -u "$expected" "$tmp" || true
		elif [ -s "$tmp" ]; then
			echo "unexpected output:"
			cat "$tmp"
		fi
		failed=$((failed + 1))
	fi
done

echo "$passed passed, $failed failed"
[ "$failed" -eq 0 ]
