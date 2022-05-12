#!/usr/bin/bash
counter=1
while [ $counter -le 20 ]
do
    echo $counter
    #./throughput_benchmark 625 is_unaligned &
    #./throughput_benchmark 625 &
    counter=$(( $counter + 1 ))
done

wait
echo All done

wait
