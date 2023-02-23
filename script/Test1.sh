#! /bin/bash
filename=$(basename $0 .sh)
export STM_CONFIG=NOrec
STMs=("LLT")
Thread=(4 8 16 32)
Ops=(1 100 200 300 400 500 600 700 800 900 1000 1100 1200 1300 1400 1500 1600 1700 1800 1900 2000 2100 2200 2300 2400 2500 2600 2700 2800 2900 3000 3100 3200 3300 3400 3500 3600 3700 3800 3900 4000 4100 4200 4300 4400 4500 4600 4700 4800 4900 5000 5100 5200 5300 5400 5500 5600 5700 5800 5900 6000 6100 6200 6300 6400 6500 6600 6700 6800 6900 7000 7100 7200 7300 7400 7500 7600 7700 7800 7900 8000 8100 8200 8300 8400 8500 8600 8700 8800 8900 9000 9100 9200 9300 9400 9500 9600 9700 9800 9900 10000)
#Ops=(1 10 20 30 40 50 60 70 80 90 100 110 120 130 140 150 160 170 180 190 200)
oo="./${filename}_output.txt"
oo2="./${filename}_final.txt"
tt="5"
cat /dev/null > $oo
cat /dev/null > $oo2

# baseline
for stms in ${STMs[@]}
do
    echo "STM_CONFIG=$stms"
    export STM_CONFIG=$stms
    for threads in ${Thread[@]}
    do
        for ops in ${Ops[@]}
        do
            echo "thread=$threads op=$ops"
            ../bench/${filename}SSB64 -p $threads -O $ops -d $tt>> $oo
        done
    done
done

# our design without opt
our="DelayByteEager"
echo "STM_CONFIG=$our"
export STM_CONFIG=$our
for threads in ${Thread[@]}
do
    for ops in ${Ops[@]}
    do
        echo "thread=$threads op=$ops"
        ../bench/${filename}SSB64 -p $threads -O $ops -d $tt>> $oo
    done
done

#our design with ops
for threads in ${Thread[@]}
do
    for ops in ${Ops[@]}
    do
        echo "thread=$threads op=$ops"
        ../bench/${filename}DelaySSB64 -p $threads -O $ops -d $tt >> $oo
    done
done


cat $oo | grep throughput > $oo2
