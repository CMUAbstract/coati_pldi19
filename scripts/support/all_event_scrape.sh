#!/bin/bash

#invoke with arguments <sys> <app> <power(1/0)> or just <power(1/0)>
if [ -z $3 ]; then
  declare -a dirs=("buffi" "coati")
  powr=$1
else
  declare -a dirs=("$1")
  powr=$3
fi

if [ $powr -eq 0 ]; then
  powr_dir="harv"
else
  powr_dir="cont"
fi

declare -a well_behaved_apps=("bc" "ar" "rsa" )
base_dir="/tmp/artifact_results"
# run through dirs
for dir in "${dirs[@]}"
  do
    echo "Start analyzing $dir results"
  # run through well behaved apps
  for app in "${well_behaved_apps[@]}"
    do
    # double check that we want to run this one
    if [ -n $3 ] ; then
      if [ $app != $2 ] ; then
        continue;
      fi
    fi
    # run through all trials
    i=1
    err_line=$(grep -m 1 events ${base_dir}/${dir}/${powr_dir}/${app}_run${i}.txt)
    echo "$err_line"
    if [ ! -z "$err_line" ]; then
      events=$(echo $err_line | grep -o -E '^[0-9]+')
      echo "ev: $events"
      str=$(echo $err_line | grep -o -E 'events\s[0-9]+')
      missed=$(echo $str | grep -o -E '[0-9]+')
      echo "missed: $missed"
    else
      echo "Error! coati bc run $i did not complete"
    fi
    echo "$i, $events, $missed"
    echo "$i, $events, $missed" >> ${base_dir}/${dir}/${powr_dir}/${app}_${dir}_events.csv
  done
done

# Now scrape the apps where we can't just use the beginning anchor to grab
# numbers
declare -a misbehaving_apps=("cem" "cuckoo" "blowfish")
declare -a trigger_words=("finish:" "final:" "done!" )
# run through dirs
for dir in "${dirs[@]}"
  do
  # inc follows app so we can index into trigger_words
  inc=0
  for app in "${misbehaving_apps[@]}"
    do
    # double check that we want to run this one
    if [ -n $3 ] ; then
      if [ $app != $2 ] ; then
        break;
      fi
    fi
      i=1
      #err_line=$(grep -m 1 finish: harvested/${dir}/${app}_run${i}.txt)
      word=${trigger_words[$inc]}
      echo "$word"
      err_line=$(grep -m 1 "$word" ${base_dir}/${dir}/${powr_dir}/${app}_run${i}.txt)
      #err_line=$(grep -m ${trigger_words[$inc]} harvested/${dir}/${app}_run${i}.txt)
      echo "Err line = $err_line"
      echo "${trigger_words[$inc]}"
      if [ ! -z "$err_line" ]; then
        str=$(echo $err_line | grep -o -E "${trigger_words[$inc]}\s[0-9]+")
        events=$(echo $str | grep -o -E '[0-9]+')
        echo "ev: $events"
        str=$(echo $err_line | grep -o -E 'events\s[0-9]+')
        missed=$(echo $str | grep -o -E '[0-9]+')
        echo "missed: $missed"
      else
        echo "Error! ${dir} ${app} run $i did not complete"
      fi
      echo "$i, $events, $missed"
      echo "$i, $events, $missed" >> ${base_dir}/${dir}/${powr_dir}/${app}_${dir}_events.csv
    # increment inc since we're going to the next app
    let inc+=1
  done
done


