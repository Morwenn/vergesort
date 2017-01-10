## vergesort fallbacks

More than a full-fledged sorting algorithm, vergesort is more of an optimization layer built on the
top of another sorting algorithm to take advantage of already sorted or reverse-sorted sequences in
the collection to sort. Therefore, instead of comparing the pdqsort-backed version of the algorithm
to other sorting algorithms (as in the README), it's actually more interesting to compare how a
sorting algorithms behaves with and without the vergesort optimization layer. To show off, we will
compare it to several of the fastest or most renowned sorting algorithms, and see how it behaves
when trying to sort data with several different patterns.

### The patterns

*In this section, consider that n is the size of the collection to sort.*

The following common patterns will be used to see how different sorting algorithms behave with and
without the vergesort layer on the top of them:
* *Shuffled*: the collection is filled with ascending values, then shuffled.
* *Shuffled (16 values)*: the collection is filled with values between 0 and 15, then shuffled.
* *All equal*: the collection is filled with a single value repeated n times.
* *Ascending*: the collection is filled with ascending values.
* *Descending*: the collection is filled with descending values.
* *Pipe organ*: half of the collection is filled with ascending values, then the other half is
filled with equivalent descending values.
* *Push front*: the collection is filled with ascending values, then the smallest value is
appended at the end of the collection.
* *Push middle*: the collection is filled with ascending values, then a value corresponding to the
middle of the collection is appended at the end of the collection.

The patterns above are common patterns used to test sorting algorithms. Median-of-3 pivot quicksort
is notably known to display a quadratic behaviour for the *pipe organ*, *push front* and *push
middle* patterns.

The following patterns have be specifically designed to test vergesort:
* *Ascending sawtooth+*: the collection if filled with ascending runs whose size is slightly
larger than *n / log n*, thus maximizing the number of runs vergesort is able to identify.
* *Descending sawtooth+*: the collection if filled with descending runs whose size is slightly
larger than *n / log n*, thus maximizing the number of runs vergesort is able to identify.
* *Ascending sawtooth-**: the collection if filled with ascending runs whose size is slightly
smaller than *n / log n*, thus maximizing the amount of comparisons that vergesort has to perform
before falling back to another sorting algorithm.
* *Descending sawtooth-*: the collection if filled with descending runs whose size is slightly
smaller than *n / log n*, thus maximizing the amount of comparisons that vergesort has to perform
before falling back to another sorting algorithm.
* *Vergesort killer*: the collection is filled with descending runs whose size is slightly larger
than *n / log n*, interleaved with runs slightly smaller than *n / log n* whose pattern corresponds
to the worst case of the fallback sorting algorithm, thus somehow maximizing the amount of runs to
merge, and ensuring that the fallback algorithm will be as slow as possible for every sub-sequence
it has to sort. Note that due to the way it works, this pattern fills the collection differently
depending on the fallback algorithm.

The following patterns will generally make vergesort fall back to the other sorting algorithm after
a single scan of the collection:
* *Shuffled*
* *Shuffled (16 values)*
* *Ascending sawtooth-*
* *Descending sawtooth-*

The following patterns shouldn't ever make vergesort use its fallback algorithm (except maybe once
before the merging step, mostly because the data left to sort can't make a run big enough for
vergesort):
* *All equal*
* *Ascending*
* *Descending*
* *Pipe organ*
* *Push front*
* *Push middle*
* *Ascending sawtooth+*
* *Descending sawtooth+*

The *vergesort killer* pattern is the only one showcased here with which vergesort identifies
several sorted runs that can be used as is, and still calls the fallback algorithm on other
sub-sequences.

### Results

The following graphs show how several sorting algorithms behave with or without the vergesort layer
when used to sort collections filled with the patterns described in the previous section.

#### `std::sort`

*This benchmark uses `std::sort` as implemented in libstdc++, MinGW 6.1.0.*

![std::sort vs. vergesort](https://i.imgur.com/FHlXWzt.png)

Vergesort successfully beats raw `std::sort` for patterns against which median-of-3 quicksort is
weak. In these cases, `std::sort` probably falls back to heapsort, which explains why it is that
slow. However, the *vergesort killer* properly bears its name here: `std::sort` is so slow against
the pipe organ pattern that it outweights the benefit of identifying presorted runs; the difference
is also explained by the fact that the *vergesort killer* pattern doesn't trigger a pipe organ-like
problem for raw `std::sort` over the whole collection.

#### `std::stable_sort`

*This benchmark uses `std::stable_sort` as implemented in libstdc++, MinGW 6.1.0.*

![std::stable_sort vs. vergesort](https://i.imgur.com/qDziwhH.png)

Vergesort beats raw `std::stable_sort` whenever it does something else than merely falling back to
it. `std::stable_sort` is even slower without vergesort for the *vergesort killer* pattern: it
seems that the shuffled sub-sequences slow it down enough to make vergesort's runs identification
worth it.

#### timsort

*This benchmark uses timsort as implemented by Goro Fuji in [gfx/cpp-TimSort](https://github.com/gfx/cpp-TimSort).*

![timsort vs. vergesort](https://i.imgur.com/hq4mYuh.png)

Since timsort is already designed to take advantage of already sorted ascending and descending
runs, the vergesort layer doesn't offer much: it is slightly faster for the *ascensing sawtooth+*
and *descending sawtooth+* patterns, but that's pretty much it.

On the other hand, its means that for virtually every pattern vergesort is designed to handle, it
does it with the same efficiency as timsort, without suffering from its huge cost for truly random
data, since it is otpimized to give up real fast in such a case.

#### spreadsort

*This benchmark uses spreadsort as implemented by Steven J. Ross in [Boost.Sort](http://www.boost.org/doc/libs/1_63_0/libs/sort/doc/html/index.html).*

![spreadsort vs. vergesort](https://i.imgur.com/CxHNavn.png)

Vergesort beats raw spreadsort whenever it does something else than merely falling back to it,
including for the *vergesort killer* pattern. From the results, it looks like the *n / log n*
heuristic of vergesort could even be lowered to achieve even better performance. That said, I
didn't manage to use the tune.pl script that comes along with spreadsort and that is supposed to
provide better compile-time constants to tune spreadsort for the current architecture.

#### pdqsort

*This benchmark uses pdqsort as implemented by Orson Peters in [orlp/pdqsort](https://github.com/orlp/pdqsort).*

![pdqsort vs.vergesort](https://i.imgur.com/bnso4V2.png)

Vergesort beats raw pdqsort whenever it does something else than merely falling back to it.
Surprisingly enough, it also beats it for the *vergesort killer* pattern, probably because the pipe
organ pattern isn't as bad as with `std::sort`, and thus does not slow down the algorithm enough to
outweight the benefits of recognizing runs.

That said, pdqsort can still hypothetically call heapsort, but its pattern-defeating optimization
makes it hard to construct such a pattern, considering it's even possible.

#### ska_sort

*This benchmark uses ska_sort as implemented by Malte Skarupke in [skarupke/ska_sort](https://github.com/skarupke/ska_sort).*

![ska_sort vs. vergesort](https://i.imgur.com/ydWn0BL.png)

As with almost every other sorting algorithm, vergesort beats raw ska_sort for most patterns it has
been designed to handle, which is pretty interesting considering that ska_sort is less adaptative
than other sorting algorithms, despite its excellent performance for almost everything. That said,
the *vergesort killer* pattern has a quite high cost (139, vs. 127 for raw ska_sort in the image
above), which might hypothetically constitute an untimely bottleneck in some applications.

### Conclusions

We can draw three main lessons from the benchmarks above:
* Vergesort beats almost everything else for the patterns it has been designed to handle, making it
a valuable optimization layer to add on the top of many sorting algorithms.
* When it can't take advantage of existing presortedness at all, it is extremely fast to fall back
to another sorting algorithm. Even when it scanned the whole collection before giving up, the
performance difference is lost in the noise (my best guess is that it has branch prediction on its
side as long as the sub-sequence is sorted, and falls back at the first branch misprediction, thus
obliviating the potential problems of branch mispredictions).
* Depending on the fallback sorting algorithm, its performance for the *vergesort killer* pattern
might become a problem. That said, the performance for this pattern is apparently never slower than
the the performance for the collection's own killer pattern.

Of course, despite this analysis, I am totally aware than I only benchmarked vergesort-augmented
algorithms on instances of `std::vector<int>` containing one million integers. The results might
be totally different depending on the nature of the collection to sort, on the type of data to
sort, and on the cost of the comparison operation.

Also, be aware that the calls to `std::reverse` make a vergesort-augmented algorithm unstable, even
when the original algorithm is stable. This could be solved with a stable *reverse* operation, but
such an operation would have a higher cost than the classic unstable one.

Anyway, depending on your needs, and if you can afford an unstable sorting algorithm eating up to
O(n) extra memory, vergesort can be an interesting tool to add to your toolbox.
