/*
 * vergesort.h - General-purpose hybrid sort
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2017 Morwenn <morwenn29@hotmail.fr>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/*
    pdqsort.h - Pattern-defeating quicksort.

    Copyright (c) 2015 Orson Peters
    Modified by Morwenn in 2015-2017 to use in vergesort

    This software is provided 'as-is', without any express or implied warranty. In no event will the
    authors be held liable for any damages arising from the use of this software.

    Permission is granted to anyone to use this software for any purpose, including commercial
    applications, and to alter it and redistribute it freely, subject to the following restrictions:

    1. The origin of this software must not be misrepresented; you must not claim that you wrote the
       original software. If you use this software in a product, an acknowledgment in the product
       documentation would be appreciated but is not required.

    2. Altered source versions must be plainly marked as such, and must not be misrepresented as
       being the original software.

    3. This notice may not be removed or altered from any source distribution.
*/
#ifndef VERGESORT_VERGESORT_H_
#define VERGESORT_VERGESORT_H_

#include <algorithm>
#include <cstddef>
#include <functional>
#include <iterator>
#include <list>
#include <utility>

#if __cplusplus >= 201103L
    #include <cstdint>
    #include <type_traits>
    #define VERGESORT_PREFER_MOVE(x) std::move(x)
#else
    #define VERGESORT_PREFER_MOVE(x) (x)
#endif

namespace vergesort
{
namespace detail
{
#if __cplusplus >= 201103L
    template<typename T>
    struct is_default_compare:
        std::false_type
    {};

    template<typename T>
    struct is_default_compare<std::less<T>>:
        std::true_type
    {};

    template<typename T>
    struct is_default_compare<std::greater<T>>:
        std::true_type
    {};
#endif

    // Returns floor(log2(n)), assumes n > 0
    template<typename Integer>
    Integer log2(Integer n)
    {
        Integer log = 0;
        while (n >>= 1) {
            ++log;
        }
        return log;
    }

    template<typename BidirectionalIterator>
    BidirectionalIterator
    prev(BidirectionalIterator it)
    {
        return --it;
    }

    template<typename BidirectionalIterator>
    BidirectionalIterator
    prev(BidirectionalIterator it,
         typename std::iterator_traits<BidirectionalIterator>::difference_type n)
    {
        std::advance(it, -n);
        return it;
    }

    template<typename ForwardIterator>
    ForwardIterator
    next(ForwardIterator it)
    {
        return ++it;
    }

    template<typename ForwardIterator>
    ForwardIterator
    next(ForwardIterator it,
         typename std::iterator_traits<ForwardIterator>::difference_type n)
    {
        std::advance(it, n);
        return it;
    }

    template<typename Iterator, typename Compare>
    void iter_sort3(Iterator a, Iterator b, Iterator c,
                    Compare compare)
    {
        if (compare(*b, *a)) { std::iter_swap(a, b); }
        if (compare(*c, *b)) { std::iter_swap(b, c); }
        if (compare(*b, *a)) { std::iter_swap(a, b); }
    }

    // C++03 implementation of std::is_sorted_until
    template<typename ForwardIterator, typename Compare>
    ForwardIterator is_sorted_until(ForwardIterator first, ForwardIterator last,
                                    Compare compare)
    {
        if (first != last) {
            ForwardIterator next = first;
            while (++next != last) {
                if (compare(*next, *first)) {
                    return next;
                }
                first = next;
            }
        }
        return last;
    }

    // Sorts [begin, end) using insertion sort with the given comparison function.
    template<typename BidirectionalIterator, typename Compare>
    void insertion_sort(BidirectionalIterator begin, BidirectionalIterator end, Compare comp) {
        typedef typename std::iterator_traits<BidirectionalIterator>::value_type T;
        if (begin == end) return;

        for (BidirectionalIterator cur = detail::next(begin) ; cur != end ; ++cur) {
            BidirectionalIterator sift = cur;
            BidirectionalIterator sift_1 = detail::prev(cur);

            // Compare first so we can avoid 2 moves for an element already positioned correctly.
            if (comp(*sift, *sift_1)) {
                T tmp = VERGESORT_PREFER_MOVE(*sift);

                do {
                    *sift-- = VERGESORT_PREFER_MOVE(*sift_1);
                } while (sift != begin && comp(tmp, *--sift_1));

                *sift = VERGESORT_PREFER_MOVE(tmp);
            }
        }
    }

    enum
    {
        // Partitions below this size are sorted using insertion sort.
        insertion_sort_threshold = 24,

        // Partitions above this size use Tukey's ninther to select the pivot.
        ninther_threshold = 80,

        // When we detect an already sorted partition, attempt an insertion sort that allows this
        // amount of element moves before giving up.
        partial_insertion_sort_limit = 8,

        // Must be multiple of 8 due to loop unrolling, and < 256 to fit in unsigned char.
        block_size = 64,

        // Cacheline size, assumes power of two.
        cacheline_size = 64
    };

    // Sorts [begin, end) using insertion sort with the given comparison function. Assumes
    // *(begin - 1) is an element smaller than or equal to any element in [begin, end).
    template<typename Iter, typename Compare>
    void unguarded_insertion_sort(Iter begin, Iter end, Compare comp) {
        typedef typename std::iterator_traits<Iter>::value_type T;
        if (begin == end) return;

        for (Iter cur = begin + 1; cur != end; ++cur) {
            Iter sift = cur;
            Iter sift_1 = cur - 1;

            // Compare first so we can avoid 2 moves for an element already positioned correctly.
            if (comp(*sift, *sift_1)) {
                T tmp = VERGESORT_PREFER_MOVE(*sift);

                do { *sift-- = VERGESORT_PREFER_MOVE(*sift_1); }
                while (comp(tmp, *--sift_1));

                *sift = VERGESORT_PREFER_MOVE(tmp);
            }
        }
    }

    // Attempts to use insertion sort on [begin, end). Will return false if more than
    // partial_insertion_sort_limit elements were moved, and abort sorting. Otherwise it will
    // successfully sort and return true.
    template<typename Iter, typename Compare>
    inline bool partial_insertion_sort(Iter begin, Iter end, Compare comp) {
        typedef typename std::iterator_traits<Iter>::value_type T;
        if (begin == end) return true;

        int limit = 0;
        for (Iter cur = begin + 1; cur != end; ++cur) {
            if (limit > partial_insertion_sort_limit) return false;

            Iter sift = cur;
            Iter sift_1 = cur - 1;

            // Compare first so we can avoid 2 moves for an element already positioned correctly.
            if (comp(*sift, *sift_1)) {
                T tmp = VERGESORT_PREFER_MOVE(*sift);

                do { *sift-- = VERGESORT_PREFER_MOVE(*sift_1); }
                while (sift != begin && comp(tmp, *--sift_1));

                *sift = VERGESORT_PREFER_MOVE(tmp);
                limit += cur - sift;
            }
        }

        return true;
    }

    template<typename T>
    T* align_cacheline(T* p) {
#ifdef UINTPTR_MAX
        std::uintptr_t ip = reinterpret_cast<std::uintptr_t>(p);
#else
        std::size_t ip = reinterpret_cast<std::size_t>(p);
#endif
        ip = (ip + cacheline_size - 1) & -cacheline_size;
        return reinterpret_cast<T*>(ip);
    }

    template<typename RandomAccessIterator>
    void swap_offsets(RandomAccessIterator first, RandomAccessIterator last,
                      unsigned char* offsets_l, unsigned char* offsets_r,
                      int num, bool use_swaps) {
        typedef typename std::iterator_traits<RandomAccessIterator>::value_type T;
        if (use_swaps) {
            // This case is needed for the descending distribution, where we need
            // to have proper swapping for pdqsort to remain O(n).
            for (int i = 0; i < num; ++i) {
                std::iter_swap(first + offsets_l[i], last - offsets_r[i]);
            }
        } else if (num > 0) {
            RandomAccessIterator l = first + offsets_l[0];
            RandomAccessIterator r = last - offsets_r[0];
            T tmp(VERGESORT_PREFER_MOVE(*l)); *l = VERGESORT_PREFER_MOVE(*r);
            for (int i = 1; i < num; ++i) {
                l = first + offsets_l[i]; *r = VERGESORT_PREFER_MOVE(*l);
                r = last - offsets_r[i]; *l = VERGESORT_PREFER_MOVE(*r);
            }
            *r = VERGESORT_PREFER_MOVE(tmp);
        }
    }

    // Partitions [begin, end) around pivot *begin using comparison function comp. Elements equal
    // to the pivot are put in the right-hand partition. Returns the position of the pivot after
    // partitioning and whether the passed sequence already was correctly partitioned. Assumes the
    // pivot is a median of at least 3 elements and that [begin, end) is at least
    // insertion_sort_threshold long. Uses branchless partitioning.
    template<typename RandomAccessIterator, typename Compare>
    std::pair<RandomAccessIterator, bool>
    partition_right_branchless(RandomAccessIterator begin, RandomAccessIterator end,
                               Compare comp)
    {
        typedef typename std::iterator_traits<RandomAccessIterator>::value_type T;

        // Move pivot into local for speed.
        T pivot(VERGESORT_PREFER_MOVE(*begin));
        RandomAccessIterator first = begin;
        RandomAccessIterator last = end;

        // Find the first element greater than or equal than the pivot (the median of 3 guarantees
        // this exists).
        while (comp(*++first, pivot));

        // Find the first element strictly smaller than the pivot. We have to guard this search if
        // there was no element before *first.
        if (first - 1 == begin) while (first < last && !comp(*--last, pivot));
        else                    while (                !comp(*--last, pivot));

        // If the first pair of elements that should be swapped to partition are the same element,
        // the passed in sequence already was correctly partitioned.
        bool already_partitioned = first >= last;
        if (!already_partitioned) {
            std::iter_swap(first, last);
            ++first;
        }

        // The following branchless partitioning is derived from "BlockQuicksort: How Branch
        // Mispredictions don’t affect Quicksort" by Stefan Edelkamp and Armin Weiss.
        unsigned char offsets_l_storage[block_size + cacheline_size];
        unsigned char offsets_r_storage[block_size + cacheline_size];
        unsigned char* offsets_l = align_cacheline(offsets_l_storage);
        unsigned char* offsets_r = align_cacheline(offsets_r_storage);
        int num_l, num_r, start_l, start_r;
        num_l = num_r = start_l = start_r = 0;

        while (last - first > 2 * block_size) {
            // Fill up offset blocks with elements that are on the wrong side.
            if (num_l == 0) {
                start_l = 0;
                RandomAccessIterator it = first;
                for (unsigned char i = 0; i < block_size;) {
                    offsets_l[num_l] = i++; num_l += !comp(*it, pivot); ++it;
                    offsets_l[num_l] = i++; num_l += !comp(*it, pivot); ++it;
                    offsets_l[num_l] = i++; num_l += !comp(*it, pivot); ++it;
                    offsets_l[num_l] = i++; num_l += !comp(*it, pivot); ++it;
                    offsets_l[num_l] = i++; num_l += !comp(*it, pivot); ++it;
                    offsets_l[num_l] = i++; num_l += !comp(*it, pivot); ++it;
                    offsets_l[num_l] = i++; num_l += !comp(*it, pivot); ++it;
                    offsets_l[num_l] = i++; num_l += !comp(*it, pivot); ++it;
                }
            }
            if (num_r == 0) {
                start_r = 0;
                RandomAccessIterator it = last;
                for (unsigned char i = 0; i < block_size;) {
                    offsets_r[num_r] = ++i; num_r += comp(*--it, pivot);
                    offsets_r[num_r] = ++i; num_r += comp(*--it, pivot);
                    offsets_r[num_r] = ++i; num_r += comp(*--it, pivot);
                    offsets_r[num_r] = ++i; num_r += comp(*--it, pivot);
                    offsets_r[num_r] = ++i; num_r += comp(*--it, pivot);
                    offsets_r[num_r] = ++i; num_r += comp(*--it, pivot);
                    offsets_r[num_r] = ++i; num_r += comp(*--it, pivot);
                    offsets_r[num_r] = ++i; num_r += comp(*--it, pivot);
                }
            }

            // Swap elements and update block sizes and first/last boundaries.
            int num = std::min(num_l, num_r);
            swap_offsets(first, last, offsets_l + start_l, offsets_r + start_r,
                         num, num_l == num_r);
            num_l -= num; num_r -= num;
            start_l += num; start_r += num;
            if (num_l == 0) first += block_size;
            if (num_r == 0) last -= block_size;
        }

        int l_size = 0, r_size = 0;
        int unknown_left = (last - first) - ((num_r || num_l) ? block_size : 0);
        if (num_r) {
            // Handle leftover block by assigning the unknown elements to the other block.
            l_size = unknown_left;
            r_size = block_size;
        } else if (num_l) {
            l_size = block_size;
            r_size = unknown_left;
        } else {
            // No leftover block, split the unknown elements in two blocks.
            l_size = unknown_left/2;
            r_size = unknown_left - l_size;
        }

        // Fill offset buffers if needed.
        if (unknown_left && !num_l) {
            start_l = 0;
            RandomAccessIterator it = first;
            for (unsigned char i = 0; i < l_size;) {
                offsets_l[num_l] = i++; num_l += !comp(*it, pivot); ++it;
            }
        }
        if (unknown_left && !num_r) {
            start_r = 0;
            RandomAccessIterator it = last;
            for (unsigned char i = 0; i < r_size;) {
                offsets_r[num_r] = ++i; num_r += comp(*--it, pivot);
            }
        }

        int num = std::min(num_l, num_r);
        swap_offsets(first, last, offsets_l + start_l, offsets_r + start_r, num, num_l == num_r);
        num_l -= num; num_r -= num;
        start_l += num; start_r += num;
        if (num_l == 0) first += l_size;
        if (num_r == 0) last -= r_size;

        // We have now fully identified [first, last)'s proper position. Swap the last elements.
        if (num_l) {
            offsets_l += start_l;
            while (num_l--) std::iter_swap(first + offsets_l[num_l], --last);
            first = last;
        }
        if (num_r) {
            offsets_r += start_r;
            while (num_r--) std::iter_swap(last - offsets_r[num_r], first), ++first;
            last = first;
        }

        // Put the pivot in the right place.
        RandomAccessIterator pivot_pos = first - 1;
        *begin = VERGESORT_PREFER_MOVE(*pivot_pos);
        *pivot_pos = VERGESORT_PREFER_MOVE(pivot);

        return std::make_pair(pivot_pos, already_partitioned);
    }

    // Partitions [begin, end) around pivot *begin using comparison function comp. Elements equal
    // to the pivot are put in the right-hand partition. Returns the position of the pivot after
    // partitioning and whether the passed sequence already was correctly partitioned. Assumes the
    // pivot is a median of at least 3 elements and that [begin, end) is at least
    // insertion_sort_threshold long.
    template<class RandomAccessIterator, class Compare>
    std::pair<RandomAccessIterator, bool>
    partition_right(RandomAccessIterator begin, RandomAccessIterator end, Compare comp)
    {
        typedef typename std::iterator_traits<RandomAccessIterator>::value_type T;

        // Move pivot into local for speed.
        T pivot(VERGESORT_PREFER_MOVE(*begin));

        RandomAccessIterator first = begin;
        RandomAccessIterator last = end;

        // Find the first element greater than or equal than the pivot (the median of 3 guarantees
        // this exists).
        while (comp(*++first, pivot));

        // Find the first element strictly smaller than the pivot. We have to guard this search if
        // there was no element before *first.
        if (first - 1 == begin) while (first < last && !comp(*--last, pivot));
        else                    while (                !comp(*--last, pivot));

        // If the first pair of elements that should be swapped to partition are the same element,
        // the passed in sequence already was correctly partitioned.
        bool already_partitioned = first >= last;

        // Keep swapping pairs of elements that are on the wrong side of the pivot. Previously
        // swapped pairs guard the searches, which is why the first iteration is special-cased
        // above.
        while (first < last) {
            std::iter_swap(first, last);
            while (comp(*++first, pivot));
            while (!comp(*--last, pivot));
        }

        // Put the pivot in the right place.
        RandomAccessIterator pivot_pos = first - 1;
        *begin = VERGESORT_PREFER_MOVE(*pivot_pos);
        *pivot_pos = VERGESORT_PREFER_MOVE(pivot);

        return std::make_pair(pivot_pos, already_partitioned);
    }

    // Similar function to the one above, except elements equal to the pivot are put to the left of
    // the pivot and it doesn't check or return if the passed sequence already was partitioned.
    // Since this is rarely used (the many equal case), and in that case pdqsort already has O(n)
    // performance, no block quicksort is applied here for simplicity.
    template<typename RandomAccessIterator, typename Compare>
    RandomAccessIterator
    partition_left(RandomAccessIterator begin, RandomAccessIterator end,
                   Compare comp)
    {
        typedef typename std::iterator_traits<RandomAccessIterator>::value_type T;

        T pivot(VERGESORT_PREFER_MOVE(*begin));
        RandomAccessIterator first = begin;
        RandomAccessIterator last = end;

        while (comp(pivot, *--last));

        if (last + 1 == end) while (first < last && !comp(pivot, *++first));
        else                 while (                !comp(pivot, *++first));

        while (first < last) {
            std::iter_swap(first, last);
            while (comp(pivot, *--last));
            while (!comp(pivot, *++first));
        }

        RandomAccessIterator pivot_pos = last;
        *begin = VERGESORT_PREFER_MOVE(*pivot_pos);
        *pivot_pos = VERGESORT_PREFER_MOVE(pivot);

        return pivot_pos;
    }

    template<typename RandomAccessIterator, typename Compare, bool Branchless>
    void pdqsort_loop(RandomAccessIterator begin, RandomAccessIterator end,
                      Compare comp, int bad_allowed, bool leftmost = true)
    {
        typedef typename std::iterator_traits<RandomAccessIterator>::difference_type diff_t;

        // Use a while loop for tail recursion elimination.
        while (true) {
            diff_t size = end - begin;

            // Insertion sort is faster for small arrays.
            if (size < insertion_sort_threshold) {
                if (leftmost) insertion_sort(begin, end, comp);
                else unguarded_insertion_sort(begin, end, comp);
                return;
            }

            // Choose pivot as median of 3 or pseudomedian of 9.
            diff_t s2 = size / 2;
            if (size > ninther_threshold) {
                iter_sort3(begin, begin + s2, end - 1, comp);
                iter_sort3(begin + 1, begin + (s2 - 1), end - 2, comp);
                iter_sort3(begin + 2, begin + (s2 + 1), end - 3, comp);
                iter_sort3(begin + (s2 - 1), begin + s2, begin + (s2 + 1), comp);
                std::iter_swap(begin, begin + s2);
            } else {
                iter_sort3(begin + s2, begin, end - 1, comp);
            }

            // If *(begin - 1) is the end of the right partition of a previous partition operation
            // there is no element in [begin, end) that is smaller than *(begin - 1). Then if our
            // pivot compares equal to *(begin - 1) we change strategy, putting equal elements in
            // the left partition, greater elements in the right partition. We do not have to
            // recurse on the left partition, since it's sorted (all equal).
            if (!leftmost && !comp(*(begin - 1), *begin)) {
                begin = partition_left(begin, end, comp) + 1;
                continue;
            }

            // Partition and get results.
            std::pair<RandomAccessIterator, bool> part_result;
            if (Branchless) part_result = partition_right_branchless(begin, end, comp);
            else            part_result = partition_right(begin, end, comp);
            RandomAccessIterator pivot_pos = part_result.first;
            bool already_partitioned = part_result.second;

            // Check for a highly unbalanced partition.
            diff_t l_size = pivot_pos - begin;
            diff_t r_size = end - (pivot_pos + 1);
            bool highly_unbalanced = l_size < size / 8 || r_size < size / 8;

            // If we got a highly unbalanced partition we shuffle elements to break many patterns.
            if (highly_unbalanced) {
                // If we had too many bad partitions, switch to heapsort to guarantee O(n log n).
                if (--bad_allowed == 0) {
                    std::make_heap(begin, end, comp);
                    std::sort_heap(begin, end, comp);
                    return;
                }

                if (l_size >= insertion_sort_threshold) {
                    std::iter_swap(begin,             begin + l_size / 4);
                    std::iter_swap(pivot_pos - 1, pivot_pos - l_size / 4);

                    if (l_size > ninther_threshold) {
                        std::iter_swap(begin + 1,         begin + (l_size / 4 + 1));
                        std::iter_swap(begin + 2,         begin + (l_size / 4 + 2));
                        std::iter_swap(pivot_pos - 2, pivot_pos - (l_size / 4 + 1));
                        std::iter_swap(pivot_pos - 3, pivot_pos - (l_size / 4 + 2));
                    }
                }

                if (r_size >= insertion_sort_threshold) {
                    std::iter_swap(pivot_pos + 1, pivot_pos + (1 + r_size / 4));
                    std::iter_swap(end - 1,                   end - r_size / 4);

                    if (r_size > ninther_threshold) {
                        std::iter_swap(pivot_pos + 2, pivot_pos + (2 + r_size / 4));
                        std::iter_swap(pivot_pos + 3, pivot_pos + (3 + r_size / 4));
                        std::iter_swap(end - 2,             end - (1 + r_size / 4));
                        std::iter_swap(end - 3,             end - (2 + r_size / 4));
                    }
                }
            } else {
                // If we were decently balanced and we tried to sort an already partitioned
                // sequence try to use insertion sort.
                if (already_partitioned && partial_insertion_sort(begin, pivot_pos, comp)
                                        && partial_insertion_sort(pivot_pos + 1, end, comp)) return;
            }

            // Sort the left partition first using recursion and do tail recursion elimination for
            // the right-hand partition.
            pdqsort_loop<RandomAccessIterator, Compare, Branchless>(
                begin, pivot_pos, comp, bad_allowed, leftmost);
            begin = pivot_pos + 1;
            leftmost = false;
        }
    }

    template<typename RandomAccessIterator, typename Compare>
    void pdqsort(RandomAccessIterator begin, RandomAccessIterator end, Compare comp)
    {
        if (begin == end) return;

#if __cplusplus >= 201103L
        pdqsort_loop<RandomAccessIterator, Compare,
            is_default_compare<typename std::decay<Compare>::type>::value &&
            std::is_arithmetic<typename std::iterator_traits<RandomAccessIterator>::value_type>::value>(
            begin, end, comp, log2(end - begin));
#else
        pdqsort_loop<RandomAccessIterator, Compare, false>(
            begin, end, comp, log2(end - begin));
#endif
    }

    // partial application structs for partition
    template<typename T, typename Compare>
    struct partition_pivot_left
    {
        const T& pivot;
        Compare compare;

        partition_pivot_left(const T& pivot, const Compare& compare):
            pivot(pivot),
            compare(compare)
        {}

        bool operator()(const T& elem) const
        {
            return compare(elem, pivot);
        }
    };

    template<typename T, typename Compare>
    struct partition_pivot_right
    {
        const T& pivot;
        Compare compare;

        partition_pivot_right(const T& pivot, const Compare& compare):
            pivot(pivot),
            compare(compare)
        {}

        bool operator()(const T& elem) const
        {
            return not compare(pivot, elem);
        }
    };

    template<typename ForwardIterator, typename Compare>
    void quicksort(ForwardIterator first, ForwardIterator last,
                   typename std::iterator_traits<ForwardIterator>::difference_type size,
                   Compare compare)
    {
        typedef typename std::iterator_traits<ForwardIterator>::value_type value_type;
        typedef typename std::iterator_traits<ForwardIterator>::difference_type difference_type;
        using std::swap;

        // If the collection is small, fall back to insertion sort
        if (size < 32) {
            insertion_sort(first, last, compare);
            return;
        }

        // Choose pivot as median of 9
        ForwardIterator it1 = detail::next(first, size / 8);
        ForwardIterator it2 = detail::next(it1, size / 8);
        ForwardIterator it3 = detail::next(it2, size / 8);
        ForwardIterator middle = detail::next(it3, size/2 - 3*(size/8));
        ForwardIterator it4 = detail::next(middle, size / 8);
        ForwardIterator it5 = detail::next(it4, size / 8);
        ForwardIterator it6 = detail::next(it5, size / 8);
        ForwardIterator last_1 = detail::next(it6, size - size/2 - 3*(size/8) - 1);

        iter_sort3(first, it1, it2, compare);
        iter_sort3(it3, middle, it4, compare);
        iter_sort3(it5, it6, last_1, compare);
        iter_sort3(it1, middle, it4, compare);

        // Put the pivot at position prev(last) and partition
        std::iter_swap(middle, last_1);
        ForwardIterator middle1 = std::partition(
            first, last_1,
            partition_pivot_left<value_type, Compare>(*last_1, compare)
        );

        // Put the pivot in its final position and partition
        std::iter_swap(middle1, last_1);
        ForwardIterator middle2 = std::partition(
            detail::next(middle1), last,
            partition_pivot_right<value_type, Compare>(*middle1, compare)
        );

        // Recursive call: heuristic trick here: in real world cases,
        // the middle partition is more likely to be smaller than the
        // right one, so computing its size should generally be cheaper
        difference_type size_left = std::distance(first, middle1);
        difference_type size_middle = std::distance(middle1, middle2);
        difference_type size_right = size - size_left - size_middle;

        // Recurse in the smallest partition first to limit the call
        // stack overhead
        if (size_left > size_right) {
            swap(first, middle2);
            swap(middle1, last);
            swap(size_left, size_right);
        }
        quicksort(first, middle1, size_left, compare);
        quicksort(middle2, last, size_right,
                  VERGESORT_PREFER_MOVE(compare));
    }

    // In-place merge where [first, middle1), [middle1, middle2)
    // and [middle2, last) are sorted. The two in-place merges are
    // done in the order that should result in the smallest number
    // of comparisons
    template<typename BidirectionalIterator, typename Compare>
    void inplace_merge3(BidirectionalIterator first, BidirectionalIterator middle1,
                        BidirectionalIterator middle2, BidirectionalIterator last,
                        Compare compare)
    {
        if (std::distance(first, middle1) < std::distance(middle2, last)) {
            std::inplace_merge(first, middle1, middle2, compare);
            std::inplace_merge(first, middle2, last, compare);
        } else {
            std::inplace_merge(middle1, middle2, last, compare);
            std::inplace_merge(first, middle1, last, compare);
        }
    }

    // vergesort for bidirectional iterators
    template<typename BidirectionalIterator, typename Compare>
    void vergesort(BidirectionalIterator first, BidirectionalIterator last,
                   Compare compare, std::bidirectional_iterator_tag)
    {
        typedef typename std::iterator_traits<BidirectionalIterator>::difference_type difference_type;

        difference_type dist = std::distance(first, last);
        if (dist < 80) {
            // vergesort is inefficient for small collections
            quicksort(first, last, dist, compare);
            return;
        }

        // Limit under which quicksort is used
        int unstable_limit = dist / log2(dist);

        // Beginning of an unstable partition, last if the
        // previous partition is stable
        BidirectionalIterator begin_unstable = last;

        // Size of the unstable partition
        std::size_t size_unstable = 0;

        // Pair of iterators to iterate through the collection
        BidirectionalIterator next = detail::is_sorted_until(first, last, compare);
        BidirectionalIterator current = next;
        --current;

        while (true) {
            BidirectionalIterator begin_rng = current;

            // Decreasing range
            while (next != last) {
                if (compare(*current, *next)) break;
                ++current;
                ++next;
            }

            // Reverse and merge
            dist = std::distance(begin_rng, next);
            if (dist > unstable_limit) {
                if (begin_unstable != last) {
                    quicksort(begin_unstable, begin_rng, size_unstable, compare);
                    std::reverse(begin_rng, next);
                    std::inplace_merge(begin_unstable, begin_rng, next, compare);
                    std::inplace_merge(first, begin_unstable, next, compare);
                    begin_unstable = last;
                    size_unstable = 0;
                } else {
                    std::reverse(begin_rng, next);
                    std::inplace_merge(first, begin_rng, next, compare);
                }
            } else {
                size_unstable += dist;
                if (begin_unstable == last) begin_unstable = begin_rng;
            }

            if (next == last) break;

            ++current;
            ++next;

            begin_rng = current;

            // Increasing range
            while (next != last) {
                if (compare(*next, *current)) break;
                ++current;
                ++next;
            }

            // Merge
            dist = std::distance(begin_rng, next);
            if (dist > unstable_limit) {
                if (begin_unstable != last) {
                    quicksort(begin_unstable, begin_rng, size_unstable, compare);
                    std::inplace_merge(begin_unstable, begin_rng, next, compare);
                    std::inplace_merge(first, begin_unstable, next, compare);
                    begin_unstable = last;
                    size_unstable = 0;
                } else {
                    std::inplace_merge(first, begin_rng, next, compare);
                }
            } else {
                size_unstable += dist;
                if (begin_unstable == last) begin_unstable = begin_rng;
            }

            if (next == last) break;

            ++current;
            ++next;
        }

        if (begin_unstable != last) {
            quicksort(begin_unstable, last, size_unstable, compare);
            std::inplace_merge(first, begin_unstable, last, compare);
        }
    }

    template<typename RandomAccessIterator, typename Compare>
    void vergesort(RandomAccessIterator first, RandomAccessIterator last,
                   Compare compare, std::random_access_iterator_tag)
    {
        typedef typename std::iterator_traits<RandomAccessIterator>::difference_type difference_type;
        difference_type dist = std::distance(first, last);

        if (dist < 80) {
            // Vergesort is inefficient for small collections
            pdqsort(first, last, compare);
            return;
        }

        // Limit under which pdqsort is used to sort a sub-sequence
        const difference_type unstable_limit = dist / detail::log2(dist);

        // Vergesort detects big runs in ascending or descending order,
        // and remember where each run ends by storing the end iterator
        // of each run in this list, then it merges everything in the end
        std::list<RandomAccessIterator> runs;

        // Beginning of an unstable partition, or last if the previous
        // partition is stable
        RandomAccessIterator begin_unstable = last;

        // Pair of iterators to iterate through the collection
        RandomAccessIterator current = first;
        RandomAccessIterator next = detail::next(first);

        while (true) {
            // Beginning of the current sequence
            RandomAccessIterator begin_range = current;

            // If the last part of the collection to sort isn't
            // big enough, consider that it is an unstable sequence
            if (std::distance(next, last) <= unstable_limit) {
                if (begin_unstable == last) {
                    begin_unstable = begin_range;
                }
                break;
            }

            // Set backward iterators
            std::advance(current, unstable_limit);
            std::advance(next, unstable_limit);

            // Set forward iterators
            RandomAccessIterator current2 = current;
            RandomAccessIterator next2 = next;

            if (compare(*next, *current)) {
                // Found a decreasing sequence, move iterators
                // to the limits of the sequence
                while (current != begin_range) {
                    --current;
                    --next;
                    if (compare(*current, *next)) break;
                }
                if (compare(*current, *next)) ++current;

                ++current2;
                ++next2;
                while (next2 != last) {
                    if (compare(*current2, *next2)) break;
                    ++current2;
                    ++next2;
                }

                // Check whether we found a big enough sorted sequence
                if (std::distance(current, next2) >= unstable_limit) {
                    std::reverse(current, next2);
                    if (std::distance(begin_range, current) && begin_unstable == last) {
                        begin_unstable = begin_range;
                    }
                    if (begin_unstable != last) {
                        pdqsort(begin_unstable, current, compare);
                        runs.push_back(current);
                        begin_unstable = last;
                    }
                    runs.push_back(next2);
                } else {
                    // Remember the beginning of the unsorted sequence
                    if (begin_unstable == last) {
                        begin_unstable = begin_range;
                    }
                }
            } else {
                // Found an increasing sequence, move iterators
                // to the limits of the sequence
                while (current != begin_range) {
                    --current;
                    --next;
                    if (compare(*next, *current)) break;
                }
                if (compare(*next, *current)) ++current;

                ++current2;
                ++next2;
                while (next2 != last) {
                    if (compare(*next2, *current2)) break;
                    ++current2;
                    ++next2;
                }

                // Check whether we found a big enough sorted sequence
                if (std::distance(current, next2) >= unstable_limit) {
                    if (std::distance(begin_range, current) && begin_unstable == last) {
                        begin_unstable = begin_range;
                    }
                    if (begin_unstable != last) {
                        pdqsort(begin_unstable, current, compare);
                        runs.push_back(current);
                        begin_unstable = last;
                    }
                    runs.push_back(next2);
                } else {
                    // Remember the beginning of the unsorted sequence
                    if (begin_unstable == last) {
                        begin_unstable = begin_range;
                    }
                }
            }

            if (next2 == last) break;

            current = detail::next(current2);
            next = detail::next(next2);
        }

        if (begin_unstable != last) {
            // If there are unsorted elements left, sort them
            runs.push_back(last);
            pdqsort(begin_unstable, last, compare);
        }

        if (runs.size() < 2) return;

        // Merge runs pairwise until there aren't runs left
        do {
            RandomAccessIterator begin = first;
            for (typename std::list<RandomAccessIterator>::iterator it = runs.begin() ;
                 it != runs.end() && it != detail::prev(runs.end()) ;
                 ++it) {
                std::inplace_merge(begin, *it, *detail::next(it), compare);

                // Remove the middle iterator and advance
                it = runs.erase(it);
                begin = *it;
            }
        } while (runs.size() > 1);
    }
}}

namespace vergesort
{
    template<typename BidirectionalIterator, typename Compare>
    void vergesort(BidirectionalIterator first, BidirectionalIterator last,
                   Compare compare)
    {
        typedef typename std::iterator_traits<BidirectionalIterator>::iterator_category category;
        detail::vergesort(first, last, compare, category());
    }

    template<typename BidirectionalIterator>
    void vergesort(BidirectionalIterator first, BidirectionalIterator last)
    {
        typedef typename std::iterator_traits<BidirectionalIterator>::value_type value_type;
        vergesort(first, last, std::less<value_type>());
    }
}

#undef VERGESORT_PREFER_MOVE

#endif // VERGESORT_VERGESORT_H_
