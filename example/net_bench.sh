#!/bin/sh

C4=$HOME/build-col/src/c4i/c4i

PORT1=11000
PORT2=11001

LOCALHOST=`hostname`
SELF1="tcp:$LOCALHOST:$PORT1"
SELF2="tcp:$LOCALHOST:$PORT2"

set -b

$C4 -p $PORT1 ./net_loop.olg &
#sleep 10

$C4 -p $PORT2 -s "ping(\"$SELF1\", \"$SELF2\", 0);" ./net_loop.olg
