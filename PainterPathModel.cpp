#include "PainterPathModel.h"

//Qt includes
#include <QLineF>
#include <QPolygonF>

using namespace cwSketch;

PainterPathModel::PainterPathModel(QObject *parent)
    : AbstractPainterPathModel(parent)
    , m_penLineModel(nullptr)
{

    // In PainterPathModel’s constructor we watch for changes to the activeLineIndex:
    connect(this, &PainterPathModel::activeLineIndexChanged, this, [this]() {
        // 1) Rebuild the geometry for the newly active stroke:
        updateActivePath();

        // 2) Refresh its stroke‑width (in case it’s changed since we first built it):
        updateActivePathStrokeWidth();

        // 3) If there was a previously active stroke (m_previousActivePath ≥ 0),
           //    finalize it by merging it into m_finishedPaths:
        if (m_previousActivePath >= 0) {
            addOrUpdateFinishPath(m_previousActivePath);
        }

        // 4) Remember the new active index so next time we can finalize *this* one:
        m_previousActivePath = m_activeLineIndex;
    });
}


/**
 * @brief Returns the total number of painter‐path entries in this model.
 *
 * The model always has one “active” path (at row 0), plus one entry
 * for each batch of finished paths in m_finishedPaths.
 *
 * @param parent Unused—this is a list model.
 * @return The count = 1 (active path) + m_finishedPaths.size().
 */
int PainterPathModel::rowCount(const QModelIndex &parent) const {
    Q_UNUSED(parent);
    return m_finishedPaths.size() + m_finishLineIndexOffset;
}

/**
 * @brief Provides the painter path or stroke width for a given row.
 *
 * Row 0 corresponds to the “active” path; rows ≥1 map into the
 * m_finishedPaths list (offset by m_finishLineIndexOffset).
 *
 * @param index   The model index (row must be in [0, rowCount()-1]).
 * @param role    One of PainterPathRole or StrokeWidthRole.
 * @return A QVariant containing either:
 *         - QPainterPath (for PainterPathRole)
 *         - double stroke width (for StrokeWidthRole)
 *         If the index is out of range or the role isn’t recognized,
 *         returns an invalid QVariant.
 */
// QVariant PainterPathModel::data(const QModelIndex &index, int role) const {
//     if (index.row() < 0 || index.row() >= rowCount()) {
//         return QVariant();
//     }

//     auto path = [this](const QModelIndex& index) -> const Path& {
//         if (index.row() == 0) {
//             return m_activePath;
//         } else  {
//             return m_finishedPaths.at(index.row() - m_finishLineIndexOffset);
//         }
//     };

//     switch(role) {
//     case PainterPathRole:
//         return QVariant::fromValue(path(index).painterPath);
//     case StrokeWidthRole:
//         return path(index).strokeWidth;
//     }

//     return QVariant();
// }

/**
 * @brief Returns the role‐to‐name mapping used by QML.
 *
 * The returned hash tells Qt’s view system which string identifiers
 * correspond to our custom roles (PainterPathRole and StrokeWidthRole).
 * In QML delegates you can then write something like:
 *
 *   delegate: Shape {
 *     property variant path: model.painterPath
 *     property real  width: model.strokeWidthRole
 *   }
 *
 * @return A QHash mapping each role enum to its QByteArray name.
 */
// QHash<int, QByteArray> PainterPathModel::roleNames() const {
//     return { {PainterPathRole, "painterPath"},
//         {StrokeWidthRole, "strokeWidthRole"}
//     };
// }

/**
 * @brief Returns the source PenLineModel supplying raw pen strokes.
 *
 * This model holds the list of PenLine objects whose points
 * are converted into QPainterPaths by PainterPathModel.
 *
 * @return Pointer to the current QAbstractItemModel (PenLineModel),
 *         or nullptr if none is set.
 */
QAbstractItemModel* PainterPathModel::penLineModel() const {
    return m_penLineModel;
}

/**
 * @brief Sets the source PenLineModel and wires up update logic.
 *
 * Disconnects any existing model, then stores and reconnects to the new one.
 * When rows are inserted in the PenLineModel:
 *  - If inserting points into the active line, it incrementally adds those points to the active path.
 *  - Otherwise, it rebuilds or finalizes the appropriate finished path.
 * When dataChanged is emitted for LineRole:
 *  - The active path is updated if its line has changed.
 *  - All finished paths are rebuilt if another line changed.
 *
 * @param penLineModel  The QAbstractItemModel (a PenLineModel) that provides raw pen strokes.
 *                      Pass nullptr to clear the current model.
 * @see addPointToActivePath(), updateActivePath(), addOrUpdateFinishPath(), rebuildFinalPath()
 */
void PainterPathModel::setPenLineModel(QAbstractItemModel* penLineModel) {
    if (m_penLineModel != penLineModel) {
        // Disconnect old model
        if (m_penLineModel) {
            disconnect(this, nullptr, m_penLineModel, nullptr);
        }

        m_penLineModel = penLineModel;

        if (m_penLineModel) {
            // Handle new points or new lines inserted
            connect(m_penLineModel, &PenLineModel::rowsInserted, this,
                    [this](const QModelIndex &parent, int first, int last) {
                        if (parent != QModelIndex()) {
                            // Points added to an existing pen line
                            Q_ASSERT(first >= 0);
                            Q_ASSERT(first < m_penLineModel->rowCount(parent));
                            Q_ASSERT(last >= 0);
                            Q_ASSERT(last < m_penLineModel->rowCount(parent));
                            Q_ASSERT(first <= last);

                            if (parent.row() == m_activeLineIndex) {
                                // If only appending to the active stroke, add incrementally
                                int pointCount = m_penLineModel->rowCount(parent);
                                if (last == pointCount - 1 && pointCount > 2) {
                                    for (int i = first; i <= last; ++i) {
                                        auto idx = m_penLineModel->index(i, 0, parent);
                                        PenPoint p = idx.data(PenLineModel::PenPointRole).value<PenPoint>();
                                        addPointToActivePath(p);
                                    }
                                } else {
                                    // Otherwise rebuild the entire active path
                                    updateActivePath();
                                }
                            } else {
                                // A finished stroke changed
                                addOrUpdateFinishPath(parent.row());
                            }
                        } else {
                            // Entire new lines added to the model
                            Q_ASSERT(first >= 0);
                            Q_ASSERT(first < m_penLineModel->rowCount());
                            Q_ASSERT(last >= 0);
                            Q_ASSERT(last < m_penLineModel->rowCount());
                            Q_ASSERT(first <= last);

                            for (int row = first; row <= last; ++row) {
                                if (row == m_activeLineIndex) {
                                    updateActivePath();
                                    updateActivePathStrokeWidth();
                                } else {
                                    addOrUpdateFinishPath(row);
                                }
                            }
                        }
                    });

            // Handle modifications to existing pen lines
            connect(m_penLineModel, &PenLineModel::dataChanged, this,
                    [this](const QModelIndex &topLeft,
                           const QModelIndex &bottomRight,
                           const QList<int> &roles) {
                        if (roles.contains(PenLineModel::LineRole)) {
                            // If the active line changed, rebuild it
                            if (m_activeLineIndex >= topLeft.row() &&
                                m_activeLineIndex <= bottomRight.row()) {
                                updateActivePath();
                            }
                            // If any other line changed, rebuild finished batches
                            if (!(m_activeLineIndex >= topLeft.row() &&
                                  m_activeLineIndex <= bottomRight.row())) {
                                rebuildFinalPath();
                            }
                        }
                    });

            connect(m_penLineModel, &QAbstractItemModel::modelReset,
                    this, [this]() {
                        m_previousActivePath = m_activeLineIndex;
                        m_activeLineIndex = -1;
                        rebuildFinalPath();
                    });
        }

        emit penLineModelChanged();
    }
}


/**
 * @brief Converts a normalized pressure value into a half‑width for stroking.
 *
 * This method asserts that the input pressure is within [0.0, 1.0], then
 * linearly interpolates between m_minHalfWidth and m_maxHalfWidth, and
 * finally applies the overall m_widthScale factor.
 *
 * @param point  The PenPoint whose pressure ∈ [0,1].
 * @return       The half‑width (in scene units) to use for that pressure.
 */
double PainterPathModel::pressureToLineHalfWidth(const PenPoint &point) const
{
    Q_ASSERT(point.pressure >= 0.0);
    Q_ASSERT(point.pressure <= 1.0);

    // linear interpolation: min + pressure * (max – min)
    double interpolated = point.pressure * (m_maxHalfWidth - m_minHalfWidth)
                          + m_minHalfWidth;

    // apply global scale factor
    return m_widthScale * interpolated;
}

/**
 * @brief Appends a new point to the “active” painter path.
 *
 * If m_activePath.strokeWidth > 0, draws a simple lineTo().
 * Otherwise, for a variable‑width brush it:
 *   1. Retrieves the latest PenLine and its points.
 *   2. Smooths pressure over m_smoothingPressureWindow.
 *   3. Computes perpendicular lines at each smoothed point.
 *   4. Builds linePolygon: beginCap, topLine, endCap, bottomLine.
 *   5. Replaces m_activePath.painterPath with that polygon.
 * Emits dataChanged() for row 0 when done.
 *
 * @param newPoint  The PenPoint (position + pressure) to add.
 */
void PainterPathModel::addPointToActivePath(const PenPoint &newPoint)
{
    if (m_activePath.strokeWidth > 0.0) {
        // Fixed‑width stroke: just draw to the new point
        m_activePath.painterPath.lineTo(newPoint.position);
    } else {
        // Variable‑width stroke: rebuild the full polygon
        int last = m_penLineModel->rowCount() - 1;
        PenLine penLine = m_penLineModel
                              ->data(m_penLineModel->index(last, 0), PenLineModel::LineRole)
                              .value<PenLine>();
        QVector<PenPoint> linePoints = penLine.points;

        int endSmooth   = linePoints.size() - 1;
        int beginSmooth = std::max(0, endSmooth - m_smoothingPressureWindow + 1);
        QVector<PenPoint> smoothPoints =
            smoothPressure(linePoints, beginSmooth, endSmooth);

        QVector<QLineF> newPerpendicularLines;
        newPerpendicularLines.reserve(smoothPoints.size());
        for (int i = 0; i < smoothPoints.size(); ++i) {
            newPerpendicularLines.append(
                perpendicularLineAt(smoothPoints, i)
                );
        }

        QPolygonF linePolygon;
        linePolygon.reserve(
            m_activePath.beginCap.size()
            + m_activePath.topLine.size() + 1
            + m_activePath.bottomLine.size() + 1
            + m_endPointTessellation
            );

        // begin cap
        linePolygon.append(m_activePath.beginCap);

        // update and append topLine
        for (int i = 1; i < newPerpendicularLines.size() - 1; ++i) {
            m_activePath.topLine[beginSmooth + i] =
                newPerpendicularLines.at(i).p1();
        }
        m_activePath.topLine.append(newPerpendicularLines.last().p1());
        linePolygon.append(m_activePath.topLine);

        // end cap
        linePolygon.append(endCap(smoothPoints, newPerpendicularLines.last()));

        // update and append bottomLine
        for (int i = 1; i < newPerpendicularLines.size() - 1; ++i) {
            int bottomLineIndex =
                m_activePath.bottomLine.size() - beginSmooth - i;
            m_activePath.bottomLine[bottomLineIndex] =
                newPerpendicularLines.at(i).p2();
        }
        m_activePath.bottomLine.prepend(newPerpendicularLines.last().p2());
        linePolygon.append(m_activePath.bottomLine);

        m_activePath.painterPath.clear();
        m_activePath.painterPath.addPolygon(linePolygon);

        QModelIndex activeIndex = index(0);
        emit dataChanged(activeIndex, activeIndex, { PainterPathRole });
    }

    // Ensure views are updated
    QModelIndex activeIndex = index(0);
    emit dataChanged(activeIndex, activeIndex, { PainterPathRole });
}

/**
 * @brief Builds and appends the painter path for a single pen line.
 *
 * This method reads the PenLine at @a modelRow from m_penLineModel,
 * then:
 *   - If the pen’s width (LineWidthRole) > 0, it constructs a simple
 *     centerline via moveTo()/lineTo() and appends it to @a path.
 *   - Otherwise, for a variable‑width stroke, it:
 *       1. Generates smoothed pressure values for all points.
 *       2. Computes perpendicular offset lines at each point.
 *       3. Constructs a full polygon: begin cap, top edge, end cap, bottom edge.
 *       4. Appends that polygon to @a path.
 *   - If @a modelRow equals m_activeLineIndex, also caches the
 *     beginCap, topLine, and bottomLine vectors into m_activePath.
 *
 * @param path      The QPainterPath to which this line’s geometry is added.
 * @param modelRow  The row in m_penLineModel whose PenLine to render.
 */
void PainterPathModel::addLinePolygon(QPainterPath &path, int modelRow)
{

    auto beginCap = [this](const QVector<PenPoint>& points, const QLineF& perpendicularLine) {
        Q_ASSERT(points.size() >= 2);
        auto firstPoint = points.at(0);
        auto normal = firstPoint.position - points.at(1).position;
        return capFromNormal(normal, perpendicularLine, pressureToLineHalfWidth(firstPoint));
    };

    // auto endCap = [this](const QVector<PenPoint>& points, const QLineF& perpendicularLine) {
    //     Q_ASSERT(points.size() >= 2);
    //     auto lastIndex = points.size() - 1;
    //     auto lastPoint = points.at(lastIndex);
    //     return cap(lastPoint.position, points.at(lastIndex - 1).position, perpendicularLine);
    // };

    auto polygon = [this, beginCap](const QVector<PenPoint>& linePoints)->VariableWidthLine {
        Q_ASSERT(linePoints.size() >= 2);

        //Generate the perpendicularLines that give width to the pen line
        QVector<QLineF> perpendicularLines;
        perpendicularLines.reserve(linePoints.size());
        for(int i = 0; i < linePoints.size(); i++) {
            auto line = perpendicularLineAt(linePoints, i);
            perpendicularLines.append(line);
        }

        //Generate the polygon
        QPolygonF polygon;
        polygon.reserve((perpendicularLines.size() + m_endPointTessellation) * 2 );

        //Begin cap
        auto beginCapPoints = beginCap(linePoints, perpendicularLines.at(0));
        polygon.append(beginCapPoints);

        //Go forward around the top edge of the polygon
        // qDebug() << "PerpendicularLines:" << perpendicularLines.size();
        QVector<QPointF> topLine;
        topLine.reserve(perpendicularLines.size());
        for(const auto& line : perpendicularLines) {
            topLine.append(line.p1());
        }

        //Add to the final polygon
        polygon.append(topLine);

        //End cap
        auto lastPerpendicularIndex = perpendicularLines.size() - 1;
        polygon.append(endCap(linePoints, perpendicularLines.at(lastPerpendicularIndex)));

        //Go backward around the bottom edge of the polygon
        QVector<QPointF> bottomLine;
        bottomLine.reserve(perpendicularLines.size());
        for(auto it = perpendicularLines.crbegin(); it != perpendicularLines.crend(); ++it) {
            bottomLine.append(it->p2());
        }
        polygon.append(bottomLine);

        return VariableWidthLine {
            beginCapPoints,
            topLine,
            bottomLine,
            polygon
        };
    };

    auto smoothPressure = [this](const QVector<PenPoint>& linePoints) {
        return this->smoothPressure(linePoints, 0, linePoints.size() - 1);
    };

    auto centerline = [](const QVector<PenPoint>& linePoints) {
        QPainterPath path;
        path.reserve(linePoints.size());
        if(linePoints.size() > 0) {
            path.moveTo(linePoints.first().position);
            for(int i = 1; i < linePoints.size(); i++) {
                path.lineTo(linePoints.at(i).position);
            }
        }
        return path;
    };

    auto penLine = m_penLineModel->data(m_penLineModel->index(modelRow, 0), PenLineModel::LineRole).value<PenLine>();
    double penWidth = m_penLineModel->data(m_penLineModel->index(modelRow, 0), PenLineModel::LineWidthRole).toDouble();
    auto linePoints = penLine.points;

    if(linePoints.size() >= 2) {
        if(penWidth > 0.0) {
            path.addPath(centerline(linePoints));
        } else {
            // auto polygonLine = polygon(smoothPressure(linePoints));
            auto polygonLine = polygon(linePoints);
            path.addPolygon(polygonLine.polygon);

            if(modelRow == m_activeLineIndex) {
                //Cache the variablWidthline data
                m_activePath.beginCap = polygonLine.beginCap;
                m_activePath.topLine = polygonLine.topLine;
                m_activePath.bottomLine = polygonLine.bottomLine;
            }
        }

        //Generate the perpendicularLines that give width to the pen line
        // QVector<QLineF> perpendicularLines;
        // perpendicularLines.reserve(linePoints.size());
        // for(int i = 0; i < linePoints.size(); i++) {
        //     auto line = perpendicularLineAt(linePoints, i);
        //     // perpendicularLines.append(line);
        //     path.moveTo(line.p1());
        //     path.lineTo(line.p2());
        // }

        // path = path.simplified();
    }

}

/**
 * @brief Computes the perpendicular offset line at a given point in a stroke.
 *
 * For the point at @a index within @a points:
 *   - If both neighbors exist, treats this as a “middle” point:
 *       * Computes the bisector angle between the segments to left and right.
 *       * Creates two half‑width lines (top and bottom) along that bisector.
 *   - If only one neighbor exists (start or end of the stroke):
 *       * Constructs a normal to the single segment and offsets by halfWidth.
 *   - Asserts that there are at least two valid points.
 *
 * @param points  The full list of PenPoint for the stroke.
 * @param index   The position within @a points to compute the offset.
 * @return        A QLineF whose p1() = the top offset end, p2() = the bottom offset end.
 */
QLineF PainterPathModel::perpendicularLineAt(const QVector<PenPoint>& points, int index) const
{
    Q_ASSERT(index >= 0);
    Q_ASSERT(index < points.size());
    Q_ASSERT(points.size() >= 2);

    const auto left = index - 1 < 0 ? PenPoint() : points.at(index - 1);
    const auto mid = points.at(index);
    const auto right = index + 1 >= points.size() ? PenPoint() : points.at(index + 1);

    QLineF topLine;
    QLineF bottomLine;

    double halfWidth = pressureToLineHalfWidth(mid);

    //    qDebug() << "Null?:" << !left.isNull() << !right.isNull();
    if(!left.isNull() && !right.isNull()) {
        //A middle segment
        QLineF leftLine(mid.position, left.position);
        QLineF rightLine(mid.position, right.position);
        double angleBetween = leftLine.angleTo(rightLine);
        double halfAngle = angleBetween * 0.5;

        // qDebug() << "Both good!";

        topLine.setP1(mid.position);
        topLine.setAngle(leftLine.angle() + halfAngle);
        topLine.setLength(halfWidth);

        bottomLine.setP1(mid.position);
        bottomLine.setAngle(leftLine.angle() + halfAngle);
        bottomLine.setLength(-halfWidth);

        Q_ASSERT(topLine.p1() == bottomLine.p1());
        Q_ASSERT(topLine.p2() != bottomLine.p2());
    } else if(!left.isNull() || !right.isNull()){
        QLineF line;
        double direction;
        if(!left.isNull()) {
            //The start section
            line = QLineF(mid.position, left.position);
            direction = 1.0;
            // qDebug() << "left good:" << line;
        } else if(!right.isNull()) {
            line = QLineF(mid.position, right.position);
            direction = -1.0;
            // qDebug() << "Right good:" << line;
        }

        // qDebug() << "Mid:" << mid.position << right.position << line.length();
        Q_ASSERT(line.length() > 0.0);

        QLineF normalLine = line.normalVector();

        //        qDebug() << "NormalLine:" << normalLine;

        topLine = normalLine;
        topLine.setLength(direction * halfWidth);

        bottomLine = normalLine;
        bottomLine.setLength(-1.0 * direction * halfWidth);

        Q_ASSERT(topLine.p1() == bottomLine.p1());
        //        Q_ASSERT(topLine.p2() != bottomLine.p2());
    } else {
        Q_ASSERT(false); //Not enough data need to have at least 2 points
    }

    // qDebug() << "Line:" << topLine.p2() << bottomLine.p2();
    return QLineF(topLine.p2(), bottomLine.p2());
}

/**
 * @brief Completely rebuilds the “active” stroke’s painter path.
 *
 * Clears the existing m_activePath.painterPath, then re‑adds
 * the geometry for the current activeLineIndex via addLinePolygon().
 * Finally, emits dataChanged() for row 0 so any bound views refresh.
 */
void PainterPathModel::updateActivePath()
{
    // qDebug() << "Update active path!";
    m_activePath.painterPath.clear();
    addLinePolygon(m_activePath.painterPath, m_activeLineIndex);
    auto activePathIndex = index(0);
    // qDebug() << "ActivePath:" << activePathIndex.data(PainterPathRole).value<QPainterPath>().elementCount();
    emit dataChanged(activePathIndex, activePathIndex, {PainterPathRole});
}

/**
 * @brief Refreshes the stroke‑width value for the active path.
 *
 * Queries the source PenLineModel for the current LineWidthRole
 * at m_activeLineIndex. If it differs from m_activePath.strokeWidth,
 * updates the cached value and emits dataChanged() for row 0’s
 * StrokeWidthRole so any bound views update their rendering.
 */
void PainterPathModel::updateActivePathStrokeWidth()
{
    auto newStrokeWidth =  m_penLineModel->index(m_activeLineIndex, 0).data(PenLineModel::LineWidthRole).toDouble();
    // qDebug() << "NewStrokWidth:" << newStrokeWidth << m_activePath.strokeWidth;
    if(m_activePath.strokeWidth != newStrokeWidth) {
        m_activePath.strokeWidth = newStrokeWidth;
        auto activePathIndex = index(0);
        emit dataChanged(activePathIndex, activePathIndex, {StrokeWidthRole});
    }
}

/**
 * @brief Regenerates all finished‑stroke painter paths from the source model.
 *
 * This method:
 *   1. Clears the geometry of every entry in m_finishedPaths.
 *   2. Iterates all pen lines in m_penLineModel (except the active one)
 *      and calls addOrUpdateFinishPath() to rebuild or append their geometry.
 *   3. Removes any entries from m_finishedPaths whose painterPath remains empty,
 *      emitting the appropriate beginRemoveRows()/endRemoveRows() to update views.
 */
void PainterPathModel::rebuildFinalPath()
{
    // m_finishedPaths.clear();
    //Clear the QPainterPaths
    for(auto& path : m_finishedPaths) {
        path.painterPath.clear();
    }

    for(int i = 0; i < m_penLineModel->rowCount(); i++) {
        if(i != m_activeLineIndex) {
            addOrUpdateFinishPath(i);
        }
    }
    // auto finishedPathIndex = index(m_finishedPaths.size() + m_finishLineIndexOffset);

    //Remove unused finished paths
    for(auto it = m_finishedPaths.begin(); it != m_finishedPaths.end(); ) {
        if(it->painterPath.isEmpty()) {
            int index = it - m_finishedPaths.begin() + m_finishLineIndexOffset;
            beginRemoveRows(QModelIndex(), index, index);
            it = m_finishedPaths.erase(it);
            endRemoveRows();
        } else {
            it++;
        }
    }

}

/**
 * @brief Finds the index of a finished path with the given stroke width.
 *
 * Searches through m_finishedPaths for the first Path whose strokeWidth
 * equals the specified value.
 *
 * @param strokeWidth  The stroke width to match.
 * @return Iterator pointing to the matching Path, or m_finishedPaths.end()
 *         if no entry has the given stroke width.
 */
QList<PainterPathModel::Path>::iterator PainterPathModel::indexOfFinalPaths(double strokeWidth)
{
    auto it = std::find_if(m_finishedPaths.begin(), m_finishedPaths.end(), [strokeWidth](const Path& path) {
        return path.strokeWidth == strokeWidth;
    });
    return it;
}

/**
 * @brief Adds a pen line’s geometry to an existing finished path or creates a new one.
 *
 * Looks up a Path in m_finishedPaths matching the stroke width of the
 * pen line at @a penLineIndex. If found:
 *   - Appends its geometry via addLinePolygon()
 *   - Emits dataChanged() for the corresponding model row.
 * Otherwise:
 *   - Inserts a new row at the end (beginInsertRows()/endInsertRows())
 *   - Creates and appends a fresh Path with that stroke width
 *   - Builds its geometry via addLinePolygon()
 *
 * @param penLineIndex  Row in m_penLineModel whose line to finalize.
 */
void PainterPathModel::addOrUpdateFinishPath(int penLineIndex)
{
    // if(penLineIndex != m_activeLineIndex) {
    // qDebug() << "Adding finished path:" << i;
    double lineWidth = m_penLineModel->index(penLineIndex, 0).data(PenLineModel::LineWidthRole).toDouble();
    auto it = indexOfFinalPaths(lineWidth);
    int modelIndexInt = it - m_finishedPaths.begin() + m_finishLineIndexOffset;
    if(it != m_finishedPaths.end()) {
        //Add to existing line
        addLinePolygon(it->painterPath, penLineIndex);
        const auto modelIndex = index(modelIndexInt);
        emit dataChanged(modelIndex, modelIndex, {PainterPathRole});
    } else {
        //Create a new line
        int lastIndex = m_finishedPaths.size() + m_finishLineIndexOffset;
        beginInsertRows(QModelIndex(), lastIndex, lastIndex);
        Path newPath {QPainterPath(), lineWidth};
        m_finishedPaths.append(newPath);
        auto& lastPath = m_finishedPaths.last();
        addLinePolygon(lastPath.painterPath, penLineIndex);
        endInsertRows();
    }
    // }
}

/**
 * @brief Generates a semicircular cap polygon based on a stroke normal.
 *
 * Given the normal vector at a stroke endpoint and its perpendicular
 * line, this method tessellates a half‑circle of radius @a radius
 * to form an end cap. The arc’s orientation (clockwise vs. counter‑clockwise)
 * is chosen so that its midpoint faces in the direction of @a normal.
 *
 * @param normal             The vector direction of the stroke at the endpoint.
 * @param perpendicularLine  The QLineF whose endpoints define the cap’s diameter.
 * @param radius             The radius of the half‑circle (usually half the stroke width).
 * @return QVector<QPointF>  The sequence of arc points (excluding the two
 *         diameter endpoints), ready to append to a QPolygonF.
 *
 * @pre  m_endPointTessellation >= 3
 * @note Reserves (m_endPointTessellation - 2) points; the caller must
 *       also include the perpendicularLine endpoints separately.
 */
QVector<QPointF> PainterPathModel::capFromNormal(const QPointF &normal, const QLineF &perpendicularLine, double radius) const
{
    QVector<QPointF> points;
    Q_ASSERT(m_endPointTessellation >= 3);
    const int tessellation = m_endPointTessellation;
    points.reserve(tessellation - 2); //We don't include first and last points

    // The endpoints of the perpendicular line are the start and end of the arc.
    // Their midpoint is the center of the circle (which is at p2).
    const QPointF pStart = perpendicularLine.p1();
    const QPointF pEnd = perpendicularLine.p2();
    const QPointF center((pStart.x() + pEnd.x()) / 2.0, (pStart.y() + pEnd.y()) / 2.0);

    // Compute the starting angle (from center to pStart)
    const double angleStart = std::atan2(pStart.y() - center.y(), pStart.x() - center.x());

    // The half circle has two possible arcs.
    // We pick the one whose midpoint (at angleStart + π/2) lies in the direction of the given normal.
    const double candidateAngle = angleStart + M_PI / 2.0;
    const QPointF candidatePoint(center.x() + radius * std::cos(candidateAngle),
                                 center.y() + radius * std::sin(candidateAngle));

    // Compute the dot product of the candidate offset with the normal vector.
    const QPointF candidateOffset(candidatePoint.x() - center.x(), candidatePoint.y() - center.y());
    const double dot = candidateOffset.x() * normal.x() + candidateOffset.y() * normal.y();
    const bool usePositiveDirection = (dot > 0);

    // Tessellate the half circle (arc of π radians)
    // We create tessellation number of points starting from angleStart to angleStart ± π.
    for (int i = 1; i < tessellation - 1; ++i) {
        const double t = static_cast<double>(i) / (tessellation - 1);
        double angle = 0.0;
        if (usePositiveDirection) {
            angle = angleStart + t * M_PI;
            const QPointF pt(center.x() + radius * std::cos(angle),
                             center.y() + radius * std::sin(angle));
            points.push_front(pt);
        } else {
            angle = angleStart - t * M_PI;
            const QPointF pt(center.x() + radius * std::cos(angle),
                             center.y() + radius * std::sin(angle));
            points.push_back(pt);
        }
    }
    return points;
}

/**
 * @brief Builds the end‑cap for a variable‑width stroke.
 *
 * Assumes there are at least two PenPoint entries. Computes the stroke’s
 * normal vector at the last point (difference between the last two points),
 * then delegates to capFromNormal(), passing that normal, the provided
 * perpendicularLine, and a radius based on the last point’s pressure.
 *
 * @param points             The full sequence of PenPoint for this stroke.
 * @param perpendicularLine  The perpendicular offset line at the endpoint.
 * @return QVector<QPointF>  The tessellated cap points (excluding the diameter endpoints).
 */
QVector<QPointF> PainterPathModel::endCap(const QVector<PenPoint> &points, const QLineF &perpendicularLine) const
{
    Q_ASSERT(points.size() >= 2);
    auto lastIndex = points.size() - 1;
    auto lastPoint = points.at(lastIndex);
    auto normal = lastPoint.position - points.at(lastIndex - 1).position;
    return capFromNormal(normal, perpendicularLine, pressureToLineHalfWidth(lastPoint));
}

/**
 * @brief Smooths the pressure values of a range of PenPoints using a moving average.
 *
 * For each index i between @a begin and @a end (inclusive), this method
 * computes the average pressure over a sliding window of size
 * (2 * m_smoothingPressureWindow + 1), clamped to the valid range [0, lastIndex].
 * It returns a new QVector of PenPoint where each point’s pressure has been
 * replaced by this local average, but all other fields remain unchanged.
 *
 * @param points  The full sequence of PenPoint to sample from.
 * @param begin   The starting index into @a points (must satisfy 0 ≤ begin < points.size()).
 * @param end     The ending index into @a points   (must satisfy 0 ≤ end < points.size()).
 * @return        A QVector<PenPoint> of length (end – begin + 1) containing
 *                the points from @a begin to @a end with smoothed pressure values.
 *
 * @pre  begin ≤ end
 * @pre  points.size() ≥ end + 1
 */
QVector<PenPoint> PainterPathModel::smoothPressure(const QVector<PenPoint>& points, int begin, int end) const
{
    Q_ASSERT(begin >= 0);
    Q_ASSERT(begin < points.size());
    Q_ASSERT(end >= 0);
    Q_ASSERT(end < points.size());
    Q_ASSERT(begin <= end);

    auto size = end - begin;

    QVector<PenPoint> smoothPoints;
    smoothPoints.reserve(size);

    int window = m_smoothingPressureWindow;
    const int lastIndex = points.size() - 1;
    for (int i = begin; i <= end; ++i) {
        int start = std::max(0, i - window);
        int clampedEnd = std::min(lastIndex, i + window);

        double sumPressure = 0.0f;
        double count = clampedEnd - start + 1;

        for (int j = start; j <= clampedEnd; ++j) {
            sumPressure += points.at(j).pressure;
        }

        PenPoint smoothedPoint = points.at(i);
        smoothedPoint.pressure = sumPressure / count;
        smoothPoints.push_back(smoothedPoint);
    }

    return smoothPoints;
}



const AbstractPainterPathModel::Path &PainterPathModel::path(const QModelIndex &index) const
{
    if (index.row() == 0) {
        return m_activePath;
    } else  {
        return m_finishedPaths.at(index.row() - m_finishLineIndexOffset);
    }
}
