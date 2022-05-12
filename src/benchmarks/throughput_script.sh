#!/usr/bin/bash

./throughput_benchmark 30000 is_unaligned &
./throughput_benchmark 30000 is_unaligned &
./throughput_benchmark 30000 is_unaligned &
./throughput_benchmark 30000 is_unaligned &

./throughput_benchmark 30000 &
./throughput_benchmark 30000 &
./throughput_benchmark 30000 &
./throughput_benchmark 30000 &

wait
