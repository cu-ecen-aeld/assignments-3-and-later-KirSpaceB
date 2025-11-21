#!/bin/bash
set -euo pipefail

if [ $# -ne 2 ]; then
  echo "Usage: $0 <filesdir> <searchstr>"
  exit 1
fi

filesdir=$1
searchstr=$2

# Validation
if [ ! -d "$filesdir" ]; then
  echo "Error: $filesdir is not a directory"
  exit 1
fi

# Stuff
X=$(find "$filesdir" -type f | wc -l)

# Y = number of matching lines (recursively, fixed-string, skip binaries)
Y=$(grep -R --binary-files=without-match -F "$searchstr" "$filesdir" | wc -l)

echo "The number of files are $X and the number of matching lines are $Y"
