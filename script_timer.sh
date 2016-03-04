i=1
j=30
while [ $i -le $j ]
do
  ./lock_tester 3772
  (( i++ ))
  echo "scripttest ${i}:"
  sleep 3
done
