

This is Lillian's repo for experimental OrderBook optimization.

```
git clone git@github.com:lillianqin/book-project.git
cd book-project
git submodule update --init --recursive
```

Run the following to build
```
cmake --preset debug
cmake --build --preset debug
```

Run the following to invoke all tests
```
ctest --preset debug
```

Change above `debug` to `release` for release mode if you would like to enable compiler optimization.

The objective of the repo is to optimize the OrderBook class, which can be benchmarked via running the program itchbook_printer.
To measure the time of running itchbook_printer for a day of Nasdaq's raw data, build in release mode first, and then run the following command
```
/usr/bin/time ./build/release/itch50/itchbook_printer --printUpdate=false --printOther=false --date 20191230
```

After some time, the command should print something like
```
start=20191230 00:00:00.000000000 end=23:59:59.000000000
done processing book, remaining orders=0, remaining levels=0
maxNumOrders=1924078, maxNumLevels=784060
181.66user 0.88system 3:12.14elapsed 95%CPU (0avgtext+0avgdata 394564maxresident)k
0inputs+0outputs (0major+222602minor)pagefaults 0swaps
```

In total, above run took 181.66 seconds user time.  The goal is to reduce the time with better data structures for the book.
