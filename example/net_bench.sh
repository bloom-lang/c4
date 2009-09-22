#!/bin/sh

COL=$HOME/build-col/src/cli/coverlog

PORT1=11000
PORT2=11001

LOCALHOST=`hostname`
SELF1="tcp:$LOCALHOST:$PORT1"
SELF2="tcp:$LOCALHOST:$PORT2"

$COL -p $PORT1 ./net_loop.olg &

$COL -p $PORT2 -s "ping(\"$SELF2\", \"$SELF1\");" ./net_loop.olg
