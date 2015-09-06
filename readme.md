vergesort
---------

Vergesort is a new sorting algorithm which combines merge operations on almost sorted data, and
falls back to a [pattern-defeating quicksort](https://github.com/orlp/pdqsort) when the data is
not sorted enough. Just like TimSort, it achieves linear time on some patterns, generally for
almost sorted data, and should never be worse than O(n log n). This last statement has not been
proven, but the benchmarks show that it performs only a bit slower than the pattern-defeating
quicksort for shuffled data, which is its worst case, so...

    Best        Average     Worst       Memory      Stable
    n           n log n     n log n     n           No

The code being released under the MIT license (except the many bits taken from pdqsort, which
fall under the zlib license), you are free to use the code as you wish.

### Benchmark

A comparison of introsort (gcc `std::sort` at time of writing), heapsort (gcc `std::sort_heap`),
pdqsort, vergesort and timsort with various input distributions:

![Performance graph](http://i.imgur.com/RdWf27n.png)

Compiled with MinGW g++ 5.1 `-std=c++14 -O2 -march=native`.

Note that the latest two benchmarks are a bit biased: they highlight the few cases where vergesort
clearly beats both TimSort and pdqsort. These cases correspond to series of ascending (first one)
and descending (second one) patterns whose size is a bit bigger than n / log2 n where n is the
size of the collection to sort.

### The algorithm

While I never quite looked at how TimSort worked before writing vergesort, it seems that vergesort
shares a bit of its logic with TimSort. Basically, vergesort runs through the collection while it
is sorted in ascending or descending order and computes the size of the sorted sub-collection. If
it is *big enough*, then it merges it in-place with the beginning of the collection (and reverses
it first if the sub-collection is sorted in descending order).

If the sorted sub-collection is not big enough, vergesort just remembers its beginning and moves
on to the next sorted sub-collection. When it reaches a *big enough* sub-collection that is already
sorted or reverse-sorted, it calls the pattern-defeating quicksort to sort everything that is between
the remembered iterator and the beginning of the *big enough* sorted sub-collection and then merges
everything (it's a bit hard to explain, I hope that reading both this explanation and the algorithm
will make things clearer).

A sub-collection is considered *big enough* when its size is bigger than n / log2 n where n is the
size of the entire collection to sort. This heuristic is sometimes suboptimal since tests have proven
that it would be better not to fall back to the pattern-defeating quicksort in some cases, but I
couldn't find a better general-purpose heuristic, so this one will do for the time being.

### Afterword

I don't think I can thank Orson Peters enough. Not only did I use his pattern-defeating quicksort
as a fallback for not-sorted-enough collections (well, it *is* the fastest for random data while not
having the usual quicksort's complexity issues) but I also took his benchmarks. I guess that I could
thank many more people involved in the creation of sorting algorithms considering the amount of
fallback sorting algorithms used in the overall thing.
