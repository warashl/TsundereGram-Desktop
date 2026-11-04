#!/bin/bash
# Publishes a build error report to the ci-logs branch, so it can be read
# without downloading the whole job log.
set -u
LABEL="$1"
FILE="$GITHUB_WORKSPACE/$LABEL-errors.txt"
[ -f "$FILE" ] || { echo "no report for $LABEL"; exit 0; }
CONTENT=$(base64 -w0 "$FILE")
P="logs/${GITHUB_RUN_ID}-${GITHUB_RUN_ATTEMPT}-${LABEL}.txt"
gh api -X PUT "repos/$GITHUB_REPOSITORY/contents/$P" \
  -f message="CI report $LABEL for run $GITHUB_RUN_ID" \
  -f content="$CONTENT" -f branch=ci-logs >/dev/null && echo "report: $P" || echo "report upload failed"
