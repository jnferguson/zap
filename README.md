# zap
# zap.cpp: journal file editor

This allows you to edit journald log files based on specified criterion, specifically it allows you to remove log entries.

There are some caveats to be aware of:

1. Much of the code was "translated" from the systemd code base into C++, this equates to a glorified copy/paste and I did so without any regards for security. *As such, above and beyond the normal there is the potential for memory corruption errors and similar.*
2. There is a lot of functionality missing/unimplemented. I misread a man page that said fields coming from the kernel were trustworthy and interpreted it to mean that log files were tamper evident. This made me wonder how and I started writing this code and only part way through realised that I had misunderstood what it was saying. As a result, a lot of stuff is non-functional/unimplemented. This includes:
 - Compressed objects are unimplemented
 - HMACs are unimplemented
 - Tag objects are unimplemented
 - Hash verification of objects is presently broken and commented out
 - Rewritten entries are done in the order in which they existed and the output is not resorted
 - It probably breaks on big endian systems
3. It's very slow. There was a bug that I fixed and it got remarkably slower and its not clear why, however the largest performance impact is likely that I handle each log entry object multiple times, converting them in between C structure representations and C++ object representations and processing them back and forth multiple times. Modifying that would likely yield significant increases in performance
4. . It's not been extensively tested, there are likely errors that will make output log entries not match consistently with originals. This should be minimal as no modification of any sort occurs to log entries that are kept (log entries are put into a list and then based on matching criteria selectively removed with the remainder being rewritten), however your mileage may vary and you'll probably want to do some testing and review before you use it. 

Beyond that, I tried to keep the strictest of dependencies (which is why compression is presently unimplemented). Specifically, all you need is a C++11 capable compiler on a Linux system. This could be a very useful tool when and if I update it/fix some things/et cetera.

## Example usage:

```
$ make
/usr/bin/g++ -std=c++11 -Wall -Werror -pedantic -c siphash.cpp -o siphash.o
/usr/bin/g++ -std=c++11 -Wall -Werror -pedantic -c lookup3.cpp -o lookup3.o
/usr/bin/g++ -std=c++11 -Wall -Werror -pedantic -c log.cpp -o log.o
/usr/bin/g++ -std=c++11 -Wall -Werror -pedantic -c object.cpp -o object.o
/usr/bin/g++ -std=c++11 -Wall -Werror -pedantic -c journal.cpp -o journal.o
/usr/bin/g++ -std=c++11 -Wall -Werror -pedantic -c input_journal.cpp -o input_journal.o
/usr/bin/g++ -std=c++11 -Wall -Werror -pedantic -c output_journal.cpp -o output_journal.o
/usr/bin/g++ -std=c++11 -Wall -Werror -pedantic -c file.cpp -o file.o
/usr/bin/g++ -std=c++11 -Wall -Werror -pedantic -c main.cpp -o main.o
/usr/bin/g++ -o zap main.o file.o object.o journal.o input_journal.o output_journal.o log.o siphash.o lookup3.o
$ time ./zap -f ../logs/system.journal.0 -o tmp.out -F _SYSTEMD_USER_UNIT -V tracker -F _CAP_EFFECTIVE -V ffffffffff  -F _exe -V /usr/lib/systemd/systemd 
[++]: Parsing input file
[++]: Locating specified fields
[++]: Locating specified field values
[++]: Seaching for matches to specified criterion
[++]: 23 matches identified
[++]: Removing matches
[++]: Rewriting modified log into memory
[++]: Rewriting modified log to disk
[++]: Verifiying written log file
$ journalctl --verify --file ./tmp.out 
PASS: ./tmp.out
$
```
