#!/bin/sh
set -eu

if [ $# -ne 2 ]; then
    echo "Usage: $0 <filesdir> <searchstr>" >&2
    exit 1
fi

filesdir=$1
searchstr=$2

if [ ! -d "$filesdir" ]; then
    echo "Error: $filesdir is not a directory" >&2
    exit 1
fi

num_files=$(grep -rl "$searchstr" "$filesdir" | wc -l)
num_lines=$(grep -r  "$searchstr" "$filesdir" | wc -l)

echo "The number of files are ${num_files} and the number of matching lines are ${num_lines}"

