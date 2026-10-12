#!/bin/sh
set -eu

tracefile=$1
minimum=$2

awk -v minimum="$minimum" '
  /^LF:/  { lines_total += substr($0, 4) }
  /^LH:/  { lines_hit += substr($0, 4) }
  /^BRF:/ { branches_total += substr($0, 5) }
  /^BRH:/ { branches_hit += substr($0, 5) }
  END {
    line_rate = lines_total ? 100 * lines_hit / lines_total : 100
    branch_rate = branches_total ? 100 * branches_hit / branches_total : 100
    printf "coverage gate: lines %.1f%%, branches %.1f%% (minimum %d%%)\n",
           line_rate, branch_rate, minimum
    if (lines_hit * 100 < lines_total * minimum ||
        branches_hit * 100 < branches_total * minimum)
      exit 1
  }
' "$tracefile"
