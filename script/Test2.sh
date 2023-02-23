#! /bin/bash
filename=$(basename $0 .sh)
export STM_CONFIG=NOrec
STMs=("LLT ByteEager")
Thread=(1 4 8 16 32)
Ops=(200)
Loc=(0 20 40 60 80 100)
Reqs="16"
oo="./${filename}_output.txt"
oo2="./${filename}_final.txt"
cpt="./${filename}_cpt.txt"
tt="5"
cnt=0
r_cnt=0

if test -s $cpt; then
    echo "Find checkpoint file in $cpt. Continue last execution."
    r_cnt=$(<$cpt)
    echo $r_cnt
else
    echo "First time to execute this script."
    cat /dev/null > $oo
    cat /dev/null > $oo2
fi

#baseline
for stm in ${STMs[@]}
do
    export STM_CONFIG=$stm
    echo "STM_CONFIG=$stm"
    for thread in ${Thread[@]}
    do
        for ops in ${Ops[@]}
        do
            for loc in ${Loc[@]}
            do
                if [ "$cnt" -ge "$r_cnt" ]; then
                    echo "../bench/${filename}SSB64 -p $thread -O $ops -R $loc -d $tt -m $Reqs >> $oo"
                    ../bench/${filename}SSB64 -p $thread -O $ops -R $loc -d $tt -m $Reqs >> $oo
                fi
                ((cnt++))
                echo $cnt > $cpt
            done
        done
    done
done

#our alg
export STM_CONFIG=DelayByteEager
echo "STM_CONFIG=DelayByteEager"
#our alg with opt
for thread in ${Thread[@]}
do
    for ops in ${Ops[@]}
    do
        for loc in ${Loc[@]}
        do
            if [ "$cnt" -ge "$r_cnt" ]; then
                echo "../bench/${filename}SSB64 -p $thread -O $ops -R $loc -d $tt -m $Reqs >> $oo"
                ../bench/${filename}SSB64 -p $thread -O $ops -R $loc -d $tt -m $Reqs >> $oo
            fi
            ((cnt++))
            echo $cnt > $cpt
        done
    done
done

#our alg with opt
for thread in ${Thread[@]}
do
    for ops in ${Ops[@]}
    do
        for loc in ${Loc[@]}
        do
            if [ "$cnt" -ge "$r_cnt" ]; then
                echo "../bench/${filename}DelaySSB64 -p $thread -O $ops -R $loc -d $tt -m $Reqs >> $oo"
                ../bench/${filename}DelaySSB64 -p $thread -O $ops -R $loc -d $tt -m $Reqs >> $oo
            fi
            ((cnt++))
            echo $cnt > $cpt
        done
    done
done

echo "STM finished"

cat $oo | grep throughput > $oo2

rm $cpt

echo "All done!!!"

