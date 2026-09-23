#!/bin/sh

set -u

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=$(mktemp)
trap 'rm -f "$tmp"' EXIT HUP INT TERM

passed=0
failed=0

for source in "$root"/tests/*.src; do
	name=${source##*/}
	name=${name%.src}
	input="$root/tests/$name.in"
	expected="$root/tests/$name.out"

	if [ ! -f "$expected" ]; then
		echo "FAIL $name (missing expected output)"
		failed=$((failed + 1))
		continue
	fi

	if [ -f "$input" ]; then
		"$root/ifj" "$source" < "$input" > "$tmp"
	else
		"$root/ifj" "$source" < /dev/null > "$tmp"
	fi
	status=$?

	if [ "$status" -eq 0 ] &&
		[ "$(cat "$expected")" = "$(cat "$tmp")" ]; then
		echo "PASS $name"
		passed=$((passed + 1))
	else
		echo "FAIL $name (exit $status)"
		diff -u "$expected" "$tmp" || true
		failed=$((failed + 1))
	fi
done

echo "$passed passed, $failed failed"
[ "$failed" -eq 0 ]
