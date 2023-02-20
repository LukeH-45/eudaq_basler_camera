#!/usr/bin/env sh
BINPATH=../../../bin

$BINPATH/euRun -n RunControl &
sleep 1
$BINPATH/euLog &
sleep 1
$BINPATH/euCliMonitor -n DummyMonitor -t my_mon &

$BINPATH/euCliProducer -n DummyProducer -t my_pd0 &
