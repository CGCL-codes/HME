#!/bin/sh

kill -s 9 `ps -ef | grep perf | awk '{print $2}'`
