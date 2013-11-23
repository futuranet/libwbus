#!/bin/sh

echo "Plotting the file "$1

gnuplot <<EOF
set terminal png size 1024,768
set output "$(basename $1).png"
set xlabel "time"
set title "Heater run"
set grid
set autoscale
set xdata time
set timefmt "%d.%m.%y\t%H:%M:%S"
set format x "%d.%m.%y\t%H:%M:%S"
set xtics rotate by -30 out nomirror
set key horizontal outside bottom center
#plot for [i=3:17] "< iconv -f unicode -t utf8 $1 2> /dev/null |sed 's/ /_/g' 2> /dev/null |head -n 1 ; iconv -f unicode -t utf8 $1 |sed 's/ /_/g' |grep -v Date" using 1:i title columnhead with lines
plot "< iconv -f unicode -t utf8 $1 2> /dev/null |sed 's/ /_/g' 2> /dev/null |head -n 1 ; iconv -f unicode -t utf8 $1 |sed 's/ /_/g' |grep -v Date" using 1:3 title columnhead with lines, "< iconv -f unicode -t utf8 $1 2> /dev/null |sed 's/ /_/g' 2> /dev/null |head -n 1 ; iconv -f unicode -t utf8 $1 |sed 's/ /_/g' |grep -v Date" using 1:4 title columnhead with lines, "< iconv -f unicode -t utf8 $1 2> /dev/null |sed 's/ /_/g' 2> /dev/null |head -n 1 ; iconv -f unicode -t utf8 $1 |sed 's/ /_/g' |grep -v Date" using 1:5 title columnhead with lines, "< iconv -f unicode -t utf8 $1 2> /dev/null |sed 's/ /_/g' 2> /dev/null |head -n 1 ; iconv -f unicode -t utf8 $1 |sed 's/ /_/g' |grep -v Date" using 1:6 title columnhead with lines, "< iconv -f unicode -t utf8 $1 2> /dev/null |sed 's/ /_/g' 2> /dev/null |head -n 1 ; iconv -f unicode -t utf8 $1 |sed 's/ /_/g' |grep -v Date" using 1:(\$7*300) title columnhead with lines, "< iconv -f unicode -t utf8 $1 2> /dev/null |sed 's/ /_/g' 2> /dev/null |head -n 1 ; iconv -f unicode -t utf8 $1 |sed 's/ /_/g' |grep -v Date" using 1:(\$8*310) title columnhead with lines, "< iconv -f unicode -t utf8 $1 2> /dev/null |sed 's/ /_/g' 2> /dev/null |head -n 1 ; iconv -f unicode -t utf8 $1 |sed 's/ /_/g' |grep -v Date" using 1:(\$9*320) title columnhead with lines, "< iconv -f unicode -t utf8 $1 2> /dev/null |sed 's/ /_/g' 2> /dev/null |head -n 1 ; iconv -f unicode -t utf8 $1 |sed 's/ /_/g' |grep -v Date" using 1:(\$10*330) title columnhead with lines, "< iconv -f unicode -t utf8 $1 2> /dev/null |sed 's/ /_/g' 2> /dev/null |head -n 1 ; iconv -f unicode -t utf8 $1 |sed 's/ /_/g' |grep -v Date" using 1:11 title columnhead with lines, "< iconv -f unicode -t utf8 $1 2> /dev/null |sed 's/ /_/g' 2> /dev/null |head -n 1 ; iconv -f unicode -t utf8 $1 |sed 's/ /_/g' |grep -v Date" using 1:12 title columnhead with lines, "< iconv -f unicode -t utf8 $1 2> /dev/null |sed 's/ /_/g' 2> /dev/null |head -n 1 ; iconv -f unicode -t utf8 $1 |sed 's/ /_/g' |grep -v Date" using 1:(\$13*350) title columnhead with lines, "< iconv -f unicode -t utf8 $1 2> /dev/null |sed 's/ /_/g' 2> /dev/null |head -n 1 ; iconv -f unicode -t utf8 $1 |sed 's/ /_/g' |grep -v Date" using 1:14 title columnhead with lines, "< iconv -f unicode -t utf8 $1 2> /dev/null |sed 's/ /_/g' 2> /dev/null |head -n 1 ; iconv -f unicode -t utf8 $1 |sed 's/ /_/g' |grep -v Date" using 1:15 title columnhead with lines, "< iconv -f unicode -t utf8 $1 2> /dev/null |sed 's/ /_/g' 2> /dev/null |head -n 1 ; iconv -f unicode -t utf8 $1 |sed 's/ /_/g' |grep -v Date" using 1:16 title columnhead with lines, "< iconv -f unicode -t utf8 $1 2> /dev/null |sed 's/ /_/g' 2> /dev/null |head -n 1 ; iconv -f unicode -t utf8 $1 |sed 's/ /_/g' |grep -v Date" using 1:17 title columnhead with lines
#pause -1
EOF
