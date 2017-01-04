vergesort
---------

Vergesort is a new sorting algorithm which combines merge operations on almost sorted data, and
falls back to another sorting algorithm ([pattern-defeating quicksort][1] here, but it could be
anything) when the data is not sorted enough. It achieves linear time on some patterns, generally
for almost sorted data, and should never perform worse than O(n log n). This last statement has
not been formally proven, but there is an intuitive logic explained in the description of the
algorithm which makes this complexity seem honest.

    Best        Average     Worst       Memory      Stable
    n           n log n     n log n     n           No

While vergesort has been designed to work with random-access iterators, this repository also
contains an experimental version which works with bidirectional iterators. The algorithm is
slightly different and a bit slower overall. Also, it falls back on a regular median-of-9
quicksort instead of a pattern-defeating quicksort since the latter is designed to work with
random-access iterators. The complexity for the bidirectional iterators version should be as
follows:

    Best        Average     Worst       Memory      Stable
    n           n log n     n²          n           No

It should be noted that the worst case should run in O(n²) since vergesort falls back to a
median-of-9 quicksort. That said, the quicksort tends to have a worst case complexity for some
specific patterns, and the vergesort layer might be efficient against these patterns. That said,
the time complexity could be lowered to O(n log n) by replacing the quicksort by a mergesort.
Quicksort was chosen for consistency because it's in the family of pattern-defeating quicksort.

### Benchmarks

A comparison of `std::sort`, `std::stable_sort`, heapsort (`std::make_heap` + `std::sort_heap`),
pdqsort and vergesort with various input distributions for random-access iterators, when sorting
a collection of one million `int` values:

![Random-access sorts](http://i.imgur.com/sDzdCAX.png)

Note that the "sawtooth" distributions are biased: they highlight the few cases where vergesort is
designed to fully take advantage of long sorted or reverse-sorted runs to beat the other sorting
algorithms. These cases correspond to series of ascending (first one) and descending (second one)
runs whose size is a bit bigger than n / log2 n, where n is the size of the collection to sort.

The following benchmark compares a list-specific `std::list::sort`, median-of-9 quicksort, a
regular mergesort, and vergesort with various input distributions for bidirectional iterators:

![Bidirectional sorts](https://i.imgur.com/J3XYJtw.png)

These benchmarks have been compiled with MinGW g++ 6.1.0 `-std=c++1z -O2 -march=native`.

### The algorithm

*In the following sequence, we will call "run" a sorted (in ascending or descending order)
sub-sequence of the collection to sort. It can be as short as two elements.*

Vergesort is based on a very simple principle: it tries to find big runs in the collection to
sort to take advantage of the presortedness, and falls back to another sorting algorithm to handle
the sections of the collection to sort without *big enough* runs. Its best feature is its ability
to give up *really* fast in most scenarios and to fallback to another algorithm without a
noticeable overhead. More than a sorting algorithm, it's a thin layer to add on top of other
sorting algorithms.

Basically, vergesort runs through the collection while it is sorted in ascending or descending
order, and computes the size of the current run. If the run is *big enough*, then vergesort
rememberq the bounds of the run, to merge it in another step (it reverses the run first if it
is sorted in descending order). If the run is not *big enough*, vergesort just remembers its
beginning and moves on to the next run. When it reaches a *big enough* run, it calls the fallback
sorting algorithm to sort every element between the beginning of the section without *big enough*
runs and the beginning of the current *big enough* run.

Once vergesort has crossed the entire collection, there should only be fairly big sorted runs left.
At this point, vergesort then uses a k-way merge to merge all the runs and leave a fully sorted
collection.

A run is considered *big enough* when its size is bigger than n / log2 n, where n is the size of
the entire collection to sort. This heuristic is sometimes suboptimal since tests have proven that
it would be better not to fall back to the pattern-defeating quicksort in some cases, but I was
unable to find a better general-purpose heuristic, so this one will do for the time being.

### Optimization

Vergesort has one main optimization which allows to scan the collection for *big enough* runs and
to fail fast enough that it is almost unnticeable in the benchmarks: instead of going through the
collection element by element, vergesort jumps n / log2 n elements at a time, and expands iterators
to the left and to the right to check whether it is in a *big enough* run. In some cases, it allows
to detect that we are not in a *big enough* run without having to check every element and to fall
back to the pattern-defeating quicksort with barely more than log2 n comparisons. This optimization
requires jumps through the table and thus does not exist for the bidirectional version.

The k-way merge at the end of the algorithm is fairly poor and unoptimized, but the optimization
described above is sufficient to make vergesort a valuable tool to augment sorting algorithms that
are fast on average, but not so good at taking advantage of presortedness.

### Complexity

Because the bidirectional version of the algorithm is pretty slow and doesn't even benefit from the
main optimization of the algorithm, we will only analyze the random-access version of the algorithm
here.

Now I'm specifically good when its comes from complexity, but it seems pretty logical to estimate
that the complexity of vergesort is O(p + q + r), where p, q and r correspond to the following:
* p is the complexity of the step where we "run through" the collection: intuitively we've got
  O(p) = O(n) and Ω(p) = Ω(log n)
* q is the complexity of sorting unsorted subsequences in the collection. In the best case, the
  whole collection is already sorted and there's no cost. I guess that the worst case is when
  there are reverse-sorted run of size n / log2 n interleaved with unsorted sub-sequences of size
  n / log2 n too. In this case, the complexity will be dominated by the sorting operations, which
  will then be O(q) = O(log n * (n / log n) * log (n / log n)), which can be simplified to O(q) =
  O(n log (n / log n)). This complexity looks smaller than O(n log n), which is the lower bound
  for a comparison sort. That means that in this case, O(p+q+r) is actually dominated by O(r).
* r is the complexity of the k-way merge step. I couldn't find the overall complexity, but I
  expect it to be O(n log² n) when there isn't extra memory available because of the repeated calls
  to `std::inplace_merge`. Maybe you can [help with the complexity][2] :)

Basically, it would make a O(n log n) algorithm when enough memory is available, and an O(n log² n)
algorithm otherwise, just like `std::stable_sort`. That said, the algorithm needs up to O(log n)
memory to keep track of the runs, so it would probably crash sooner anyway if extra memory wasn't
available. I'm not sure what to say about its complexity now...

The space complexity is dominated by the k-way merge step that eats up n/2 memory, making it use
O(n/2 + log n) = O(n) extra memory in the worst case.


  [1]: https://github.com/orlp/pdqsort
  [2]: http://cs.stackexchange.com/q/68271/29312