/*
 * vergesort.h - General-purpose hybrid sort
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2016 Morwenn <morwenn29@hotmail.fr>
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
#ifndef VERGESORT_VERGESORT_H_
#define VERGESORT_VERGESORT_H_

// Used by several other internal headers, hence defined first
#if __cplusplus >= 201103L
    #define VERGESORT_PREFER_MOVE(x) std::move(x)
#else
    #define VERGESORT_PREFER_MOVE(x) (x)
#endif

#include <algorithm>
#include <cstddef>
#include <functional>
#include <iterator>
#include "detail/is_sorted_until.h"
#include "detail/log2.h"
#include "detail/pdqsort.h"
#include "detail/quicksort.h"

namespace vergesort
{
    namespace detail
    {
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

        // vergesort for random-access iterators
        template<typename RandomAccessIterator, typename Compare>
        void vergesort(RandomAccessIterator first, RandomAccessIterator last,
                       Compare compare, std::random_access_iterator_tag)
        {
            typedef typename std::iterator_traits<RandomAccessIterator>::difference_type difference_type;
            difference_type dist = std::distance(first, last);

            if (dist < 80) {
                // vergesort is inefficient for small collections
                pdqsort(first, last, compare);
                return;
            }

            // Limit under which pdqsort is used
            difference_type unstable_limit = dist / log2(dist);

            // Beginning of an unstable partition, last if the
            // previous partition is stable
            RandomAccessIterator begin_unstable = last;

            // Pair of iterators to iterate through the collection
            RandomAccessIterator next = detail::is_sorted_until(first, last, compare);
            if (next == last) return;
            RandomAccessIterator current = next - 1;

            while (true) {
                // Beginning of the current range
                RandomAccessIterator begin_range = current;

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

                if (compare(*current, *next)) {
                    // Found an increasing range, move iterators
                    // to the limits of the range
                    while (current != begin_range) {
                        --current;
                        --next;
                        if (compare(*next, *current)) break;
                    }
                    if (compare(*next, *current)) ++current;

                    while (next2 != last) {
                        if (compare(*next2, *current2)) break;
                        ++current2;
                        ++next2;
                    }

                    // Remember the beginning of the unsorted range
                    if (begin_unstable == last) begin_unstable = begin_range;

                    // Check whether we found a big enough sorted sequence
                    if (std::distance(current, next2) >= unstable_limit) {
                        pdqsort(begin_unstable, current, compare);
                        inplace_merge3(first, begin_unstable, current, next2, compare);
                        begin_unstable = last;
                    }
                } else {
                    // Found an decreasing range, move iterators
                    // to the limits of the range
                    while (current != begin_range) {
                        --current;
                        --next;
                        if (compare(*current, *next)) break;
                    }
                    if (compare(*current, *next)) ++current;

                    while (next2 != last) {
                        if (compare(*current2, *next2)) break;
                        ++current2;
                        ++next2;
                    }

                    // Remember the beginning of the unsorted range
                    if (begin_unstable == last) begin_unstable = begin_range;

                    // Check whether we found a big enough sorted sequence
                    if (std::distance(current, next2) >= unstable_limit) {
                        pdqsort(begin_unstable, current, compare);
                        std::reverse(current, next2);
                        inplace_merge3(first, begin_unstable, current, next2, compare);
                        begin_unstable = last;
                    }
                }

                if (next2 == last) break;

                current = current2 + 1;
                next = next2 + 1;
            }

            if (begin_unstable != last) {
                // If there are unsorted elements left,
                // sort them and merge everything
                pdqsort(begin_unstable, last, compare);
                std::inplace_merge(first, begin_unstable, last, compare);
            }
        }
    }

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
