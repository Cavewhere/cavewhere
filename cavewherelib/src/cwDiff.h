#pragma once

//Qt includes
#include <QString>
#include <QDebug>

// #include <vector>
// #include <algorithm>
// #include <functional>

// // cwDiff: utilities for Myers diff and three-way merge of sequences of T
// // T must be copyable and comparable via a provided equality predicate

// #include <vector>
// #include <functional>
// #include <stdexcept>

// // Implements Myers' diff algorithm with linear space (divide and conquer).
// // Reference: https://blog.jcoglan.com/2017/02/12/the-myers-diff-algorithm-part-1/

// // Internal struct to represent the "middle snake" for divide and conquer
// struct Snake {
//     // friend QDebug;
//     long m_xStart;
//     long m_yStart;
//     long m_xEnd;
//     long m_yEnd;
// };


// // QDebug stream operator
// inline QDebug operator<<(QDebug dbg, const Snake& snake) {
//     QDebugStateSaver saver(dbg);
//     dbg.nospace() << "Snake("
//                   << "[xStart: " << snake.m_xStart << ", "
//                   << "xEnd: " << snake.m_xEnd << "] "
//                   << "[yStart: " << snake.m_yStart << ", "
//                   << "yEnd: " << snake.m_yEnd << "])";
//     return dbg;
// }

#include <vector>
#include <algorithm>
#include <cstddef>

namespace cwDiff {

enum class EditOperation {
    Insert,
    Delete
};

template<typename T>
struct Edit {
    EditOperation operation;
    std::size_t positionOld;
    std::size_t positionNew; // Only used for inserts
    T value;
};






template<typename T, typename Iter>
std::vector<Edit<T>> diff(const Iter& beginA, const Iter& endA,
                          const Iter& beginB, const Iter& endB,
                          std::size_t indexA = 0, std::size_t indexB = 0)
{
    const std::size_t N = std::distance(beginA, endA);
    const std::size_t M = std::distance(beginB, endB);
    const std::size_t L = N + M;
    const std::size_t Z = 2 * std::min(N, M) + 2;

    if (N > 0 && M > 0) {
        int w = static_cast<int>(N) - static_cast<int>(M);
        std::vector<int> g(Z, 0);
        std::vector<int> p(Z, 0);

        for (std::size_t h = 0; h <= L / 2 + (L % 2 != 0); ++h) {
            for (int r = 0; r < 2; ++r) {
                auto& c = (r == 0) ? g : p;
                auto& d = (r == 0) ? p : g;
                int o = (r == 0) ? 1 : 0;
                int m = (r == 0) ? 1 : -1;

                for (int k = -(static_cast<int>(h) - 2 * std::max(0, static_cast<int>(h) - static_cast<int>(M)));
                     k <= static_cast<int>(h) - 2 * std::max(0, static_cast<int>(h) - static_cast<int>(N)); k += 2)
                {
                    int a;
                    if (k == -static_cast<int>(h) || (k != static_cast<int>(h) && c[(k - 1 + Z) % Z] < c[(k + 1 + Z) % Z])) {
                        a = c[(k + 1 + Z) % Z];
                    } else {
                        a = c[(k - 1 + Z) % Z] + 1;
                    }

                    int b = a - k;
                    int s = a;
                    int t = b;

                    while (a < static_cast<int>(N) && b < static_cast<int>(M) &&
                           beginA[(1 - o) * N + m * a + (o - 1)] == beginB[(1 - o) * M + m * b + (o - 1)]) {
                        ++a;
                        ++b;
                    }

                    c[(k + Z) % Z] = a;
                    int z = -(k - w);

                    if ((L % 2) == o && z >= -(static_cast<int>(h) - o) && z <= static_cast<int>(h) - o &&
                        c[(k + Z) % Z] + d[(z + Z) % Z] >= static_cast<int>(N)) {
                        int D = (2 * h - 1);
                        int x = s;
                        int y = t;
                        int u = a;
                        int v = b;

                        if (o == 0) {
                            D = (2 * h);
                            x = static_cast<int>(N) - a;
                            y = static_cast<int>(M) - b;
                            u = static_cast<int>(N) - s;
                            v = static_cast<int>(M) - t;
                        }

                        //Clamp
                        x = std::max(0, std::min(static_cast<int>(N), x));
                        y = std::max(0, std::min(static_cast<int>(M), y));
                        u = std::max(0, std::min(static_cast<int>(N), u));
                        v = std::max(0, std::min(static_cast<int>(M), v));

                        if (D > 1 || (x != u && y != v)) {
                            auto left = diff<T>(beginA, beginA + x, beginB, beginB + y, indexA, indexB);
                            auto right = diff<T>(beginA + u, endA, beginB + v, endB, indexA + u, indexB + v);
                            left.insert(left.end(), right.begin(), right.end());
                            return left;
                        } else if (M > N) {
                            auto iter = beginB + N;
                            std::vector<Edit<T>> result;
                            result.reserve(std::distance(iter, endB));
                            for (std::size_t n = 0 ;iter < endB; ++iter, ++n) {
                                result.push_back({EditOperation::Insert, indexA + N, indexB + N + n, *iter});
                            }
                            return result;
                        } else if (M < N) {
                            // std::vector<T> deleteTail(sequenceA.begin() + M, sequenceA.end());
                            auto iter = beginA + M;
                            std::vector<Edit<T>> result;
                            result.reserve(std::distance(iter, endA));
                            for (std::size_t n = 0; iter < endA; ++iter, ++n) {
                                result.push_back({EditOperation::Delete, indexA + M + n, 0, *iter});
                            }
                            return result;
                        } else {
                            return {};
                        }
                    }
                }
            }
        }
    } else if (N > 0) {
        std::vector<Edit<T>> result;
        result.reserve(N);
        for (std::size_t n = 0; n < N; ++n) {
            result.push_back({EditOperation::Delete, indexA + n, 0, beginA[n]});
        }
        return result;
    } else {
        std::vector<Edit<T>> result;
        result.reserve(M);
        for (std::size_t n = 0; n < M; ++n) {
            result.push_back({EditOperation::Insert, indexA, indexB + n, beginB[n]});
        }
        return result;
    }

    return {}; // fallback
}

template<typename T, typename Container>
std::vector<Edit<T>> diff(const Container& from, const Container& to) {
    return diff<T>(from.begin(), from.end(),
                   to.begin(), to.end());
}


template<typename T>
std::vector<T> applyEditScript(
    const std::vector<Edit<T>>& editScript,
    const std::vector<T>& s1,
    const std::vector<T>& s2)
{
    std::vector<T> newSequence;
    newSequence.reserve(s1.size() + s2.size());

    std::size_t i = 0;
    for (const auto& e : editScript) {
        // Copy unchanged items from s1 up to the next edit
        while (e.positionOld > i) {
            newSequence.push_back(s1[i]);
            ++i;
        }

        // At an edit point: either delete or insert
        if (e.positionOld == i) {
            if (e.operation == EditOperation::Delete) {
                // Skip s1[i]
                ++i;
            }
            else if (e.operation == EditOperation::Insert) {
                newSequence.push_back(s2[e.positionNew]);
            }
        }
    }

    // Copy any remaining tail of s1
    while (i < s1.size()) {
        newSequence.push_back(s1[i]);
        ++i;
    }

    return newSequence;
}
};



