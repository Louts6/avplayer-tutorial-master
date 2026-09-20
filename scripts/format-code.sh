#!/bin/bash
# 如果没有clang-format，可以通过brew安装：brew install clang-format

RootPath="$( cd "$(dirname "$0")/.." ; pwd -P )"

function FormatFile() {
    file=$1
    if [ "${file##*.}"x = "h"x ] ||
       [ "${file##*.}"x = "hpp"x ] ||
       [ "${file##*.}"x = "c"x ] ||
       [ "${file##*.}"x = "cpp"x ] ||
       [ "${file##*.}"x = "m"x ] ||
       [ "${file##*.}"x = "mm"x ];
    then
        clang-format -style=file -i $file
    fi
}

function FormatDir() {
    cd $1

    for file in $(find . -type f)
    do
        FormatFile $file
    done

    cd -
}

function format() {
    if [ -d $1 ]; then
        FormatDir $1
    elif [ -f $1 ]; then
        FormatFile $1
    fi
}


if [ $# -eq 0 ] ; then
    format $RootPath/qt
    format $RootPath/LearnAV
else
    for i in $@; do
        format $(pwd)/$i
    done
fi
