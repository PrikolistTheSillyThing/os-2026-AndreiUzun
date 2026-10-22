# Q1. How many requests failed?

grep -c FAIL access.log

# Q2. How many different pages were requested, and which?
cut -d' ' -f5 access.log | sort -u | wc -l ; cut -d' ' -f5 access.log | sort -u

# Q3. How many requests did each page get?
cut -d' ' -f5 access.log | sort | uniq -c

# Q4. Which user or users have the most failed requests, and how many?
grep FAIL access.log | cut -d' ' -f3 | sort | uniq -c | sort -rn | awk 'NR==1{m=$1} $1==m'

# Q5. How many requests did user3 make, and how many of them failed?
grep -c ' user3 ' access.log ; grep ' user3 ' access.log | grep -c FAIL

# Q6. Print the last 3 failed requests, showing only time and user.
grep FAIL access.log | tail -3 | cut -d' ' -f2,3

# Q7. Which login shells appear in /etc/passwd, and how many accounts use each?
cut -d: -f7 /etc/passwd | sort | uniq -c | sort -rn

# Q8. Why do the two counts differ by exactly one? (write your prediction first!)
ls /etc | wc -l ; ls -l /etc | wc -l

# Bonus. Failed requests where the minute ends in 0.
grep -E '10:[0-5]0 .* FAIL' access.log | wc -l
