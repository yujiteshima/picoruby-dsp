#!/bin/sh
# Host-side gate. Needs a POSIX picoruby build that carries picoruby-dsp.
#   PICORUBY=../picoruby/bin/picoruby hosttest/run.sh
set -e

DIR=$(cd "$(dirname "$0")" && pwd)
PICORUBY=${PICORUBY:-$DIR/../../picoruby/bin/picoruby}

out=$("$PICORUBY" "$DIR/smoke.rb")
echo "$out"
echo "$out" | grep -q "ALL OK" || exit 1
