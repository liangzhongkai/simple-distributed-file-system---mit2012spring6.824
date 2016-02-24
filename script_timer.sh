i=1
j=30
while [ $i -le $j ]
do
  ./lock_tester 3726
  (( i++ ))
  sleep 3
done
