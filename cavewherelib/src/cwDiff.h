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
// enum OperationType { Insert, Delete };

// template<typename T>
// struct DiffOperation {

//     DiffOperation(OperationType type, std::size_t positionOld, std::size_t positionNew)
//         : m_type(type), m_positionOld(positionOld), m_positionNew(positionNew)
//     { }

//     OperationType operationType() const { return m_type; }
//     std::size_t positionOld() const { return m_positionOld; }
//     std::size_t positionNew() const { return m_positionNew; }

// private:
//     OperationType m_type;
//     std::size_t   m_positionOld;
//     std::size_t   m_positionNew;
// };

// template<typename T>
// std::vector<DiffOperation<T>> diff(
//     const std::vector<T>& sequenceA,
//     const std::vector<T>& sequenceB,
//     std::size_t indexOld = 0,
//     std::size_t indexNew = 0)
// {
//     std::size_t lengthA     = sequenceA.size();
//     std::size_t lengthB     = sequenceB.size();
//     std::size_t sumLength   = lengthA + lengthB;
//     std::size_t maxSize     = 2 * std::min(lengthA, lengthB) + 2;

//     if (lengthA > 0 && lengthB > 0) {
//         int lengthDifference = static_cast<int>(lengthA) - static_cast<int>(lengthB);
//         std::vector<int> forwardDistance(maxSize);
//         std::vector<int> backwardDistance(maxSize);

//         int maxEditDistance = static_cast<int>(sumLength / 2 + (sumLength % 2 != 0));
//         auto modIndex = [&](int x) {
//             int m = x % static_cast<int>(maxSize);
//             return m < 0 ? m + maxSize : m;
//         };

//         for (int editDistance = 0; editDistance <= maxEditDistance; ++editDistance) {
//             for (int pass = 0; pass < 2; ++pass) {
//                 bool isForward    = (pass == 0);
//                 auto& current     = isForward ? forwardDistance : backwardDistance;
//                 auto& previous    = isForward ? backwardDistance : forwardDistance;
//                 int  orientation  = isForward ? 1 : 0;

//                 int startK = - (editDistance - 2 * std::max(0, editDistance - static_cast<int>(lengthB)));
//                 int endK   =   editDistance - 2 * std::max(0, editDistance - static_cast<int>(lengthA));

//                 for (int diagonalIndex = startK; diagonalIndex <= endK; diagonalIndex += 2) {
//                     int xCoord = (diagonalIndex == -editDistance
//                                   || (diagonalIndex != editDistance
//                                       && previous[modIndex(diagonalIndex - 1)]
//                                              < previous[modIndex(diagonalIndex + 1)]))
//                                      ? previous[modIndex(diagonalIndex + 1)]
//                                      : previous[modIndex(diagonalIndex - 1)] + 1;
//                     int yCoord = xCoord - diagonalIndex;
//                     int startX = xCoord;
//                     int startY = yCoord;

//                     // "Snake"—follow matching subsequence
//                     while (xCoord < static_cast<int>(lengthA)
//                            && yCoord < static_cast<int>(lengthB)
//                            && (orientation == 1
//                                    ? sequenceA[xCoord] == sequenceB[yCoord]
//                                    : sequenceA[lengthA - xCoord - 1]
//                                          == sequenceB[lengthB - yCoord - 1]))
//                     {
//                         ++xCoord;
//                         ++yCoord;
//                     }

//                     current[modIndex(diagonalIndex)] = xCoord;
//                     int diagonalOffsetIndex = - (diagonalIndex - lengthDifference);

//                     // Check for overlap
//                     if ((sumLength % 2) == orientation
//                         && diagonalOffsetIndex >= -(editDistance - orientation)
//                         && diagonalOffsetIndex <=  (editDistance - orientation)
//                         && current[modIndex(diagonalIndex)]
//                                    + previous[modIndex(diagonalOffsetIndex)]
//                                >= static_cast<int>(lengthA))
//                     {
//                         int editCount = orientation == 1
//                                             ? 2 * editDistance - 1
//                                             : 2 * editDistance;

//                         size_t x = std::min(lengthA, orientation == 1 ? startX : lengthA - xCoord);
//                         size_t y = std::min(lengthB, orientation == 1 ? startY : lengthB - yCoord);
//                         size_t u = std::min(lengthA, orientation == 1 ? xCoord : lengthA - startX);
//                         size_t v = std::min(lengthB, orientation == 1 ? yCoord : lengthB - startY);

//                         std::vector<DiffOperation<T>> result;
//                         if (editCount > 1 || (x != u && y != v)) {
//                             // Recurse on the first half
//                             auto firstHalf = diff<T>(
//                                 std::vector<T>(sequenceA.begin(), sequenceA.begin() + x),
//                                 std::vector<T>(sequenceB.begin(), sequenceB.begin() + y),
//                                 indexOld,
//                                 indexNew);
//                             // Recurse on the second half
//                             auto secondHalf = diff<T>(
//                                 std::vector<T>(sequenceA.begin() + u, sequenceA.end()),
//                                 std::vector<T>(sequenceB.begin() + v, sequenceB.end()),
//                                 indexOld + u,
//                                 indexNew + v);
//                             result.reserve(firstHalf.size() + secondHalf.size());
//                             result.insert(result.end(), firstHalf.begin(),  firstHalf.end());
//                             result.insert(result.end(), secondHalf.begin(), secondHalf.end());
//                         }
//                         else if (lengthB > lengthA) {
//                             result = diff<T>({},
//                                              std::vector<T>(sequenceB.begin() + lengthA, sequenceB.end()),
//                                              indexOld + lengthA,
//                                              indexNew + lengthA);
//                         }
//                         else if (lengthA > lengthB) {
//                             result = diff<T>(
//                                 std::vector<T>(sequenceA.begin() + lengthB, sequenceA.end()),
//                                 {},
//                                 indexOld + lengthB,
//                                 indexNew + lengthB);
//                         }
//                         return result;
//                     }
//                 }
//             }
//         }
//     }
//     else if (lengthA > 0) {
//         // All deletions
//         std::vector<DiffOperation<T>> result;
//         result.reserve(lengthA);
//         for (std::size_t n = 0; n < lengthA; ++n) {
//             result.emplace_back(Delete, indexOld + n, 0);
//         }
//         return result;
//     }
//     else {
//         // All insertions
//         std::vector<DiffOperation<T>> result;
//         result.reserve(lengthB);
//         for (std::size_t n = 0; n < lengthB; ++n) {
//             result.emplace_back(Insert, indexOld, indexNew + n);
//         }
//         return result;
//     }

//     return {};  // No differences
// }

// #pragma once

// #include <vector>
// #include <string>
// #include <algorithm>

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

template<typename T>
std::vector<Edit<T>> diff(const std::vector<T>& sequenceA, const std::vector<T>& sequenceB,
                          std::size_t indexA = 0, std::size_t indexB = 0)
{
    const std::size_t N = sequenceA.size();
    const std::size_t M = sequenceB.size();
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
                           sequenceA[(1 - o) * N + m * a + (o - 1)] == sequenceB[(1 - o) * M + m * b + (o - 1)]) {
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
                            std::vector<T> e1(sequenceA.begin(), sequenceA.begin() + x);
                            std::vector<T> f1(sequenceB.begin(), sequenceB.begin() + y);
                            std::vector<T> e2(sequenceA.begin() + u, sequenceA.end());
                            std::vector<T> f2(sequenceB.begin() + v, sequenceB.end());

                            auto left = diff(e1, f1, indexA, indexB);
                            auto right = diff(e2, f2, indexA + u, indexB + v);
                            left.insert(left.end(), right.begin(), right.end());
                            return left;
                        } else if (M > N) {
                            std::vector<T> insertTail(sequenceB.begin() + N, sequenceB.end());
                            std::vector<Edit<T>> result;
                            for (std::size_t n = 0; n < insertTail.size(); ++n) {
                                result.push_back({EditOperation::Insert, indexA + N, indexB + N + n, insertTail[n]});
                            }
                            return result;
                        } else if (M < N) {
                            std::vector<T> deleteTail(sequenceA.begin() + M, sequenceA.end());
                            std::vector<Edit<T>> result;
                            for (std::size_t n = 0; n < deleteTail.size(); ++n) {
                                result.push_back({EditOperation::Delete, indexA + M + n, 0, deleteTail[n]});
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
        for (std::size_t n = 0; n < N; ++n) {
            result.push_back({EditOperation::Delete, indexA + n, 0, sequenceA[n]});
        }
        return result;
    } else {
        std::vector<Edit<T>> result;
        for (std::size_t n = 0; n < M; ++n) {
            result.push_back({EditOperation::Insert, indexA, indexB + n, sequenceB[n]});
        }
        return result;
    }

    return {}; // fallback
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



