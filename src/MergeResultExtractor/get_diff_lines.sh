#!/bin/bash

commit1=$1
commit2=$2
outfile=$3


diff_files=$(git diff --name-only $commit1 $commit2)

for file in $diff_files
do
    commit1_lines=$(git show $commit1:$file | wc -l | awk '{print $1}')
    commit2_lines=$(git show $commit2:$file | wc -l | awk '{print $1}')
    echo "$file, $commit1_lines, $commit2_lines" >> $outfile
done
