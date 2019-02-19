#!/bin/bash

echo "Checking perfect"
for i in {1..3}
do
  err_line=$(grep -m 1 events cont_pwrd/perfect_ar_run$i)
  echo "$err_line"
  if [ ! -z "$err_line" ]; then
    events=$(echo $err_line | grep -o -E '^[0-9]+')
    echo "ev: $events"
    str=$(echo $err_line | grep -o -E 'events\s[0-9]+')
    missed=$(echo $str | grep -o -E '[0-9]+')
    echo "missed: $missed"
  else
    echo "Error! perfect ar run $i did not complete"
  fi
  echo "perfect , ar, $i, $events, $missed" >> cont_pwrd_events_log.csv
done

echo "Checking coati"
for i in {1..6}
do
  err_line=$(grep -m 1 events cont_pwrd/coati_ar_run$i)
  echo "$err_line"
  if [ ! -z "$err_line" ]; then
    events=$(echo $err_line | grep -o -E '^[0-9]+')
    echo "ev: $events"
    str=$(echo $err_line | grep -o -E 'events\s[0-9]+')
    missed=$(echo $str | grep -o -E '[0-9]+')
    echo "missed: $missed"
  else
    echo "Error! coati ar run $i did not complete"
  fi
  echo "coati , ar, $i, $events, $missed" >> cont_pwrd_events_log.csv
done

echo "Checking split"
for i in {1..3}
do
  err_line=$(grep -m 1 events cont_pwrd/split_ar_run$i)
  echo "$err_line"
  if [ ! -z "$err_line" ]; then
    events=$(echo $err_line | grep -o -E '^[0-9]+')
    echo "ev: $events"
    str=$(echo $err_line | grep -o -E 'events\s[0-9]+')
    missed=$(echo $str | grep -o -E '[0-9]+')
    echo "missed: $missed"
  else
    echo "Error! split ar run $i did not complete"
  fi
  echo "split , ar, $i, $events, $missed" >> cont_pwrd_events_log.csv
done

echo "Checking atomics"
for i in {1..3}
do
  err_line=$(grep -m 1 events cont_pwrd/atomics_ar_run$i)
  echo "$err_line"
  if [ ! -z "$err_line" ]; then
    events=$(echo $err_line | grep -o -E '^[0-9]+')
    echo "ev: $events"
    str=$(echo $err_line | grep -o -E 'events\s[0-9]+')
    taken=$(echo $str | grep -o -E '[0-9]+')
    echo "taken: $taken"
  else
    echo "Error! atomics ar run $i did not complete"
  fi
  echo "atomics , ar, $i, $events, $taken" >> cont_pwrd_events_log.csv
done

echo "Checking perfect"
for i in {1..3}
do
  err_line=$(grep -m 1 events cont_pwrd/perfect_bc_run$i)
  echo "$err_line"
  if [ ! -z "$err_line" ]; then
    events=$(echo $err_line | grep -o -E '^[0-9]+')
    echo "ev: $events"
    str=$(echo $err_line | grep -o -E 'events\s[0-9]+')
    missed=$(echo $str | grep -o -E '[0-9]+')
    echo "missed: $missed"
  else
    echo "Error! perfect bc run $i did not complete"
  fi
  echo "perfect , bc, $i, $events, $missed" >> cont_pwrd_events_log.csv
done


echo "Checking coati"
for i in {1..5}
do
  err_line=$(grep -m 1 events cont_pwrd/coati_bc_run$i)
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
  echo "coati , bc, $i, $events, $missed" >> cont_pwrd_events_log.csv
done

echo "Checking split"
for i in {1..3}
do
  err_line=$(grep -m 1 events cont_pwrd/split_bc_run$i)
  echo "$err_line"
  if [ ! -z "$err_line" ]; then
    events=$(echo $err_line | grep -o -E '^[0-9]+')
    echo "ev: $events"
    str=$(echo $err_line | grep -o -E 'events\s[0-9]+')
    missed=$(echo $str | grep -o -E '[0-9]+')
    echo "missed: $missed"
  else
    echo "Error! split bc run $i did not complete"
  fi
  echo "split , bc, $i, $events, $missed" >> cont_pwrd_events_log.csv
done

echo "Checking atomics"
for i in {1..3}
do
  err_line=$(grep -m 1 events cont_pwrd/atomics_bc_run$i)
  echo "$err_line"
  if [ ! -z "$err_line" ]; then
    events=$(echo $err_line | grep -o -E '^[0-9]+')
    echo "ev: $events"
    str=$(echo $err_line | grep -o -E 'events\s[0-9]+')
    taken=$(echo $str | grep -o -E '[0-9]+')
    echo "taken: $taken"
  else
    echo "Error! atomics bc run $i did not complete"
  fi
  echo "atomics , bc, $i, $events, $taken" >> cont_pwrd_events_log.csv
done

echo "Checking alpaca"
for i in {1..3}
do
  err_line=$(grep -m 1 events cont_pwrd/alpaca_bc_run$i)
  echo "$err_line"
  if [ ! -z "$err_line" ]; then
    events=$(echo $err_line | grep -o -E '^[0-9]+')
    echo "ev: $events"
    str=$(echo $err_line | grep -o -E 'events\s[0-9]+')
    taken=$(echo $str | grep -o -E '[0-9]+')
    echo "taken: $taken"
  else
    echo "Error! alpaca bc run $i did not complete"
  fi
  echo "alpaca , bc, $i, $events, $taken" >> cont_pwrd_events_log.csv
done

echo "Checking perfect"
for i in {1..3}
do
  err_line=$(grep -m 1 done cont_pwrd/perfect_blowfish_run$i)
  echo "$err_line"
  if [ ! -z "$err_line" ]; then
    str=$(echo $err_line | grep -o -E 'done!\s[0-9]+')
    events=$(echo $str | grep -o -E '[0-9]+')
    echo "ev: $events"
    str=$(echo $err_line | grep -o -E 'events\s[0-9]+')
    missed=$(echo $str | grep -o -E '[0-9]+')
    echo "missed: $missed"
  else
    echo "Error! perfect blowfish run $i did not complete"
  fi
  missed=0
  echo "perfect , blowfish, $i, $events, $missed" >> cont_pwrd_events_log.csv
done

echo "Checking coati"
for i in {1..3}
do
  err_line=$(grep -m 1 done cont_pwrd/coati_blowfish_run$i)
  echo "$err_line"
  if [ ! -z "$err_line" ]; then
    str=$(echo $err_line | grep -o -E 'done!\s[0-9]+')
    events=$(echo $str | grep -o -E '[0-9]+')
    echo "ev: $events"
    str=$(echo $err_line | grep -o -E 'events\s[0-9]+')
    missed=$(echo $str | grep -o -E '[0-9]+')
    echo "missed: $missed"
  else
    echo "Error! coati blowfish run $i did not complete"
  fi
  echo "coati , blowfish, $i, $events, $missed" >> cont_pwrd_events_log.csv
done

echo "Checking split"
for i in {1..3}
do
  err_line=$(grep -m 1 done cont_pwrd/split_blowfish_run$i)
  echo "$err_line"
  if [ ! -z "$err_line" ]; then
    str=$(echo $err_line | grep -o -E 'done!\s[0-9]+')
    events=$(echo $str | grep -o -E '[0-9]+')
    echo "ev: $events"
    str=$(echo $err_line | grep -o -E 'events\s[0-9]+')
    missed=$(echo $str | grep -o -E '[0-9]+')
    echo "missed: $missed"
  else
    echo "Error! split blowfish run $i did not complete"
  fi
  echo "split , blowfish, $i, $events, $missed" >> cont_pwrd_events_log.csv
done

echo "Checking split"
for i in {1..3}
do
  err_line=$(grep -m 1 done cont_pwrd/atomics_blowfish_run$i)
  echo "$err_line"
  if [ ! -z "$err_line" ]; then
    str=$(echo $err_line | grep -o -E 'done!\s[0-9]+')
    events=$(echo $str | grep -o -E '[0-9]+')
    echo "ev: $events"
    str=$(echo $err_line | grep -o -E 'events\s[0-9]+')
    missed=$(echo $str | grep -o -E '[0-9]+')
    echo "taken: $missed"
  else
    echo "Error! atomics blowfish run $i did not complete"
  fi
  echo "atomics , blowfish, $i, $events, $missed" >> cont_pwrd_events_log.csv
done

echo "Checking perfect"
for i in {1..3}
do
  err_line=$(grep -m 1 finish cont_pwrd/perfect_cem_run$i)
  echo "$err_line"
  if [ ! -z "$err_line" ]; then
    str=$(echo $err_line | grep -o -E 'finish:\s[0-9]+')
    events=$(echo $str | grep -o -E '[0-9]+')
    echo "ev: $events"
    str=$(echo $err_line | grep -o -E 'events\s[0-9]+')
    missed=$(echo $str | grep -o -E '[0-9]+')
    echo "missed: $missed"
  else
    echo "Error! perfect cem run $i did not complete"
  fi
  echo "perfect , cem, $i, $events, $missed" >> cont_pwrd_events_log.csv
done


echo "Checking coati"
for i in {1..5}
do
  err_line=$(grep -m 1 finish cont_pwrd/coati_cem_run$i)
  echo "$err_line"
  if [ ! -z "$err_line" ]; then
    str=$(echo $err_line | grep -o -E 'finish:\s[0-9]+')
    events=$(echo $str | grep -o -E '[0-9]+')
    echo "ev: $events"
    str=$(echo $err_line | grep -o -E 'events\s[0-9]+')
    missed=$(echo $str | grep -o -E '[0-9]+')
    echo "missed: $missed"
  else
    echo "Error! coati cem run $i did not complete"
  fi
  echo "coati , cem, $i, $events, $missed" >> cont_pwrd_events_log.csv
done

echo "Checking split"
for i in {1..3}
do
  err_line=$(grep -m 1 finish cont_pwrd/split_cem_run$i)
  echo "$err_line"
  if [ ! -z "$err_line" ]; then
    str=$(echo $err_line | grep -o -E 'finish:\s[0-9]+')
    events=$(echo $str | grep -o -E '[0-9]+')
    str=$(echo $err_line | grep -o -E 'events\s[0-9]+')
    missed=$(echo $str | grep -o -E '[0-9]+')
    echo "missed: $missed"
  else
    echo "Error! split cem run $i did not complete"
  fi
  echo "split , cem, $i, $events, $missed" >> cont_pwrd_events_log.csv
done

echo "Checking atomics"
for i in {1..3}
do
  err_line=$(grep -m 1 finish cont_pwrd/atomics_cem_run$i)
  echo "$err_line"
  if [ ! -z "$err_line" ]; then
    str=$(echo $err_line | grep -o -E 'finish:\s[0-9]+')
    events=$(echo $str | grep -o -E '[0-9]+')
    str=$(echo $err_line | grep -o -E 'events\s[0-9]+')
    missed=$(echo $str | grep -o -E '[0-9]+')
    echo "taken: $missed"
  else
    echo "Error! atomics cem run $i did not complete"
  fi
  echo "atomics , cem, $i, $events, $missed" >> cont_pwrd_events_log.csv
done

echo "Checking perfect"
for i in {1..3}
do
  err_line=$(grep -m 1 final cont_pwrd/perfect_cuckoo_run$i)
  echo "$err_line"
  if [ ! -z "$err_line" ]; then
    str=$(echo $err_line | grep -o -E 'final:\s[0-9]+')
    events=$(echo $str | grep -o -E '[0-9]+')
    echo "ev: $events"
    missed=0
    echo "missed: $missed"
  else
    echo "Error! perfect cuckoo run $i did not complete"
  fi
  echo "perfect , cuckoo, $i, $events, $missed" >> cont_pwrd_events_log.csv
done

echo "Checking coati"
for i in {1..3}
do
  err_line=$(grep -m 1 final cont_pwrd/cuckoo_new_tx_contpwrd_$i.txt)
  echo "$err_line"
  if [ ! -z "$err_line" ]; then
    str=$(echo $err_line | grep -o -E 'final:\s[0-9]+')
    events=$(echo $str | grep -o -E '[0-9]+')
    echo "ev: $events"
    missed=0
    echo "missed: $missed"
  else
    echo "Error! coati cuckoo run $i did not complete"
  fi
  echo "coati , cuckoo, $i, $events, $missed" >> cont_pwrd_events_log.csv
done

echo "Checking split"
for i in {1..3}
do
  err_line=$(grep -m 1 final cont_pwrd/split_cuckoo_2_run$i)
  echo "$err_line"
  echo "--> $i"
  if [ ! -z "$err_line" ]; then
    str=$(echo $err_line | grep -o -E 'final:\s[0-9]+')
    events=$(echo $str | grep -o -E '[0-9]+')
    echo "ev: $events"
    str=$(echo $err_line | grep -o -E 'events\s[0-9]+')
    missed=$(echo $str | grep -o -E '[0-9]+')
    echo "missed: $missed"
  else
    echo "Error! split cuckoo run $i did not complete"
  fi
  echo "split , cuckoo, $i, $events, $missed" >> cont_pwrd_events_log.csv
done

echo "Checking atomics"
for i in {1..3}
do
  err_line=$(grep -m 1 final cont_pwrd/atomics_cuckoo_2_run$i)
  echo "$err_line"
  if [ ! -z "$err_line" ]; then
    str=$(echo $err_line | grep -o -E 'final:\s[0-9]+')
    events=$(echo $str | grep -o -E '[0-9]+')
    echo "ev: $events"
    str=$(echo $err_line | grep -o -E 'events\s[0-9]+')
    taken=$(echo $str | grep -o -E '[0-9]+')
    echo "taken: $taken"
  else
    echo "Error! atomics cuckoo run $i did not complete"
  fi
  echo "atomics , cuckoo, $i, $events, $taken" >> cont_pwrd_events_log.csv
done

echo "Checking perfect"
for i in {1..3}
do
  err_line=$(grep -m 1 events cont_pwrd/perfect_rsa_run$i)
  echo "$err_line"
  if [ ! -z "$err_line" ]; then
    events=$(echo $err_line | grep -o -E '^[0-9]+')
    echo "ev: $events"
    str=$(echo $err_line | grep -o -E 'events\s[0-9]+')
    missed=$(echo $str | grep -o -E '[0-9]+')
    echo "missed: $missed"
  else
    echo "Error! perfect rsa run $i did not complete"
  fi
  echo "perfect , rsa, $i, $events, $missed" >> cont_pwrd_events_log.csv
done


echo "Checking coati"
for i in {1..3}
do
  err_line=$(grep -m 1 events cont_pwrd/coati_rsa_run$i)
  echo "$err_line"
  if [ ! -z "$err_line" ]; then
    events=$(echo $err_line | grep -o -E '^[0-9]+')
    echo "ev: $events"
    str=$(echo $err_line | grep -o -E 'events\s[0-9]+')
    missed=$(echo $str | grep -o -E '[0-9]+')
    echo "missed: $missed"
  else
    echo "Error! coati rsa run $i did not complete"
  fi
  echo "coati , rsa, $i, $events, $missed" >> cont_pwrd_events_log.csv
done

echo "Checking split"
for i in {1..3}
do
  err_line=$(grep -m 1 events cont_pwrd/split_rsa_run$i)
  echo "$err_line"
  if [ ! -z "$err_line" ]; then
    events=$(echo $err_line | grep -o -E '^[0-9]+')
    echo "ev: $events"
    str=$(echo $err_line | grep -o -E 'events\s[0-9]+')
    missed=$(echo $str | grep -o -E '[0-9]+')
    echo "missed: $missed"
  else
    echo "Error! split rsa run $i did not complete"
  fi
  echo "split , rsa, $i, $events, $missed" >> cont_pwrd_events_log.csv
done


echo "Checking atomics"
for i in {1..3}
do
  err_line=$(grep -m 1 events cont_pwrd/atomics_rsa_run$i)
  echo "$err_line"
  if [ ! -z "$err_line" ]; then
    events=$(echo $err_line | grep -o -E '^[0-9]+')
    echo "ev: $events"
    str=$(echo $err_line | grep -o -E 'events\s[0-9]+')
    missed=$(echo $str | grep -o -E '[0-9]+')
    echo "taken: $missed"
  else
    echo "Error! atomcics rsa run $i did not complete"
  fi
  echo "atomics , rsa, $i, $events, $missed" >> cont_pwrd_events_log.csv
done


