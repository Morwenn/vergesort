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

![Random-access sorts](https://i.imgur.com/Qgsga47.png)

Note that the "sawtooth" distributions are biased: they highlight the few cases where vergesort is
designed to fully take advantage of long sorted or reverse-sorted runs to beat the other sorting
algorithms. These cases correspond to series of ascending (first one) and descending (second one)
runs whose size is a bit bigger than n / log n, where n is the size of the collection to sort.

The following benchmark compares `std::list::sort`, a median-of-9 quicksort, a regular mergesort,
and vergesort with various input distributions for bidirectional iterators:

![Bidirectional sorts](https://i.imgur.com/hDTScAb.png)

These benchmarks have been compiled with MinGW g++ 6.1.0 `-std=c++1z -O2 -march=native`.

You can also find [more benchmarks](https://github.com/Morwenn/vergesort/blob/master/fallbacks.md)
with additional information, where I compared several sorting algorithms against vergesort when
using them as fallback algorithms. These benchmarks are more interesting to see what vergesort has
been designed to achieve.

### Structure of the project

The project exposes one function: `vergesort::vergesort`, which takes either two iterators, or two
iterators and a comparison function, just like `std::sort`.

```cpp
template<typename BidirectionalIterator>
void vergesort(BidirectionalIterator first, BidirectionalIterator last);

template<typename BidirectionalIterator, typename Compare>
void vergesort(BidirectionalIterator first, BidirectionalIterator last,
               Compare compare);
```

The algorithm is split into several components into the `include/vergesort` directory, which makes
it easier to analyze and maintain the algorithm. For those who only wish to use it, it the file
`vergesort.h` at the root of the directory is a self-contained version of the algorithm, in a
single header file, that you can just take, put wherever you want to, and use as is.

Everything should work with even C++03. If you want a more modern version of vergesort, with full
projections and proxy iterators support, there is one living in my [cpp-sort][3] project, along
with several other interesting sorting algorithms.

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
remembers the bounds of the run, to merge it in another step (it reverses the run first if it
is sorted in descending order). If the run is not *big enough*, vergesort just remembers its
beginning and moves on to the next run. When it reaches a *big enough* run, it calls the fallback
sorting algorithm to sort every element between the beginning of the section without *big enough*
runs and the beginning of the current *big enough* run.

Once vergesort has crossed the entire collection, there should only be fairly big sorted runs left.
At this point, vergesort then uses a k-way merge to merge all the runs and leave a fully sorted
collection.

A run is considered *big enough* when its size is bigger than n / log n, where n is the size of
the entire collection to sort. This heuristic is sometimes suboptimal since tests have proven that
it would be better not to fall back to the pattern-defeating quicksort in some cases, but I was
unable to find a better general-purpose heuristic, so this one will do for the time being.

### Optimization

Vergesort has one main optimization which allows to scan the collection for *big enough* runs and
to fail fast enough that it is almost unnticeable in the benchmarks: instead of going through the
collection element by element, vergesort jumps n / log n elements at a time, and expands iterators
to the left and to the right to check whether it is in a *big enough* run. In some cases, it allows
to detect that we are not in a *big enough* run without having to check every element and to fall
back to the pattern-defeating quicksort with barely more than log n comparisons. This optimization
requires jumps through the table and thus does not exist for the bidirectional version.

The k-way merge at the end of the algorithm is fairly poor and unoptimized, but the optimization
described above is sufficient to make vergesort a valuable tool to augment sorting algorithms that
are fast on average, but not so good at taking advantage of presortedness.

### Complexity

Because the bidirectional version of the algorithm is pretty slow and doesn't even benefit from the
main optimization of the algorithm, we will only analyze the random-access version of the algorithm
here.

The complexity of vergesort for a given collection changes depending on the extra memory available,
and corresponds to the dominating complexity of the algorithm's three main steps:
* The step where vergesort "runs through" the collection to detect runs
* The step where we sort sub-sequences of the collection that are not sorted enough
* The final step where we merge the detected runs

*In the following analysis, *n* is the size of the collection to sort.*

The first step is intuitively comprised between Ω(log n) and O(n), so it can't dominate the
complexity of vergesort.

The worst case of the second step is probably when there are ascending or descending runs of size
slightly bigger than n / log n interleaved with unsorted sub-sequences of size slightly smaller
than n / log n (we consider both to be n / log n for the analysis). In this case, the complexity
of the step will be dominated by the sorting operations. Since there are (log n) / 2 sub-sequences
to sort, the complexity will be O(log n * (n / log n) * log (n / log n)), which can be simplified
to O(n log (n / log n)), which is equivalent to O(n log n).

The complexity of the third step is that of a k-way merge, with log n sub-sequences whose size is
n / log n. Since the algorithm merges sub-sequences pairwise, it first applies (log n) / 2 times
a merge operation on sub-sequences of 2n / log n elements, then (log n) / 4 times a merge on
sub-sequences of 4n / log n elements, etc... In the end, since the number of merges is divided by
2 at each step until everything is merged, hence it performs log k such steps, where k is the
number of sub-sequences to merge, hence it performs log log n such steps.

Now the actual complexity of that third step depends on the complexity of the merge operations. We
use [`std::inplace_merge`][4], which performs between O(N) and O(N log N) comparisons depending on
the available memory. For the sake of it, we will analyze the case where it has all the memory it
wants for the merge operations, then the case where it has no extra memory available for the same
operations. The worst case for this is when there are log n sub-sequences of size n / log n to
merge.

We have previously shown that the first pass of merging operations applies (log n) / 2 times a
merge operation on sub-sequences of 2n / log n elements, which means that the complexity of that
first pass when memory is available is O((log n)/2 * 2n/log n) = O(n). The complexity of the
second pass is O((log n)/4 * 4n/log n) = O(n), etc... every pass is O(n) and we have already
shown that there are log log n such passes, so the overall complexity of that merging step when
extra memory is available is O(n log log n). The complexity of the merging step was no extra memory
is available was a bit complicated for me to compute, so you can find a Q&A [here][2] which comes
to the conclusion that the complexity of the step is O(n log n log log n).

In the end, when extra memory is available, the complexity of vergesort is dominated by the
O(n log n) complexity of the second, while it is dominated by the O(n log n log log n) complexity
of the third step when no extra memory is available. At the end of the day, the cute log log n
factor of the third step can most likely be ignored for any real-world case, and we brand vergesort
as an O(n log n) sorting algorithm.


  [1]: https://github.com/orlp/pdqsort
  [2]: http://cs.stackexchange.com/q/68271/29312
  [3]: https://github.com/Morwenn/cpp-sort
  [4]: http://en.cppreference.com/w/cpp/algorithm/inplace_merge
