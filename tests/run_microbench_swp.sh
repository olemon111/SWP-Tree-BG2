#!/bin/bash
BUILDDIR=$(dirname "$0")/../build
Loadname="ycsb-swp"

function Run() {
    dbname=$1
    loadnum=$2 
    opnum=$3
    scansize=$4
    thread=$5
    reverse=$6
    rw=$7

    # microbench_swp
    if [ $rw == "r" ]; then
        test_read $dbname $loadnum $opnum $scansize $thread $reverse
    else
        if [ $rw == "w" ]; then
            test_write $dbname $loadnum $opnum $scansize $thread $reverse
        else
            test_read_write $dbname $loadnum $opnum $scansize $thread $reverse
        fi
    fi
}

function test_write() {
    dbname=$1
    loadnum=$2 
    opnum=$3
    scansize=$4
    thread=$5
    reverse=$6

    # Write
    # rm -f /mnt/pmem1/lbl/*
    Loadname="ycsb-write"
    date | tee output/swp-${dbname}-${Loadname}-${reverse}.txt
    # gdb --args \
    ${BUILDDIR}/microbench_swp --dbname ${dbname} --load-size ${loadnum} \
    --put-size ${opnum} --get-size 0 \
    --loadstype 3 --reverse ${reverse} -t $thread | tee -a output/swp-${dbname}-${Loadname}-${reverse}.txt
    # rm -f /mnt/pmem1/lbl/*

    echo "${BUILDDIR}/microbench_swp --dbname ${dbname} --load-size ${loadnum} "\
    "--put-size ${opnum} --get-size 0 --loadstype 3 --reverse ${reverse} -t $thread"
}

function test_read() {
    dbname=$1
    loadnum=$2 
    opnum=$3
    scansize=$4
    thread=$5
    reverse=$6

    # Read
    # rm -f /mnt/pmem1/lbl/*
    Loadname="ycsb-read"
    date | tee output/swp-${dbname}-${Loadname}-${reverse}.txt
    # gdb --args \
    ${BUILDDIR}/microbench_swp --dbname ${dbname} --load-size ${loadnum} \
    --put-size 0 --get-size ${opnum} \
    --loadstype 3 --reverse ${reverse} -t $thread | tee -a output/swp-${dbname}-${Loadname}-${reverse}.txt
    # rm -f /mnt/pmem1/lbl/*

    echo "${BUILDDIR}/microbench_swp --dbname ${dbname} --load-size ${loadnum} "\
    "--put-size 0 --get-size ${opnum} --loadstype 3 --reverse ${reverse} -t $thread"
}


function test_read_write() {
    dbname=$1
    loadnum=$2 
    opnum=$3
    scansize=$4
    thread=$5
    reverse=$6

    # Read and Write
    # rm -f /mnt/pmem1/lbl/*
    Loadname="llt-read-write"
    date | tee output/swp-${dbname}-${Loadname}-${reverse}.txt
    # gdb --args \
    numactl --cpubind=0 --membind=0 ${BUILDDIR}/microbench_swp --dbname ${dbname} --load-size ${loadnum} \
    --put-size ${opnum} --get-size ${opnum} \
    --loadstype 4 --reverse ${reverse} -t $thread | tee -a output/swp-${dbname}-${Loadname}-${reverse}.txt
    # rm -f /mnt/pmem1/lbl/*

    echo "${BUILDDIR}/microbench_swp --dbname ${dbname} --load-size ${loadnum} "\
    "--put-size ${opnum} --get-size ${opnum} --loadstype 3 --reverse ${reverse} -t $thread"
}

function run_all() {
    dbs="alex lipp xindex pgm"
    for dbname in $dbs; do
        echo "Run: " $dbname
        Run $dbname $1 $2 $3 1 $5 $6
        sleep 100
    done
}

function main() {
    dbname="alex"
    loadnum=2000000
    opnum=10000000
    scansize=0
    reverse=0.99
    thread=1
    rw="r"
    if [ $# -ge 1 ]; then
        dbname=$1
    fi
    if [ $# -ge 2 ]; then
        loadnum=$2
    fi
    if [ $# -ge 3 ]; then
        opnum=$3
    fi
    if [ $# -ge 4 ]; then
        scansize=$4
    fi
    if [ $# -ge 5 ]; then
        thread=$5
    fi
    if [ $# -ge 6 ]; then
        reverse=$6
    fi
    if [ $# -ge 6 ]; then
        rw=$7
    fi
    if [ $dbname == "all" ]; then
        run_all $loadnum $opnum $scansize $thread $reverse $rw
    else
        echo "Run $dbname $loadnum $opnum $scansize $thread $reverse" $rw
        Run $dbname $loadnum $opnum $scansize $thread $reverse $rw
    fi 
}

# main alex 2000000 10000000 0 1 0 a
# main lipp 2000000 10000000 0 1 0 a
main xindex 2000000 10000000 0 1 0 a