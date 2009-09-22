#!/bin/sh

COL=$HOME/build-col/src/cli/coverlog

PORT1=11000
PORT2=11001

LOCALHOST=`hostname`
SELF1="tcp:$LOCALHOST:$PORT1"
SELF2="tcp:$LOCALHOST:$PORT2"

set -b

$COL -p $PORT1 ./net_loop.olg &
#sleep 10

$COL -p $PORT2 -s "ping(\"$SELF1\", \"$SELF2\", 0);" ./net_loop.olg
