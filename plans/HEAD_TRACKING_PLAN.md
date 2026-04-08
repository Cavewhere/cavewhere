# Head Tracking for 3D Parallax Effect

Webcam-based head tracking to create an immersive off-axis projection in CaveWhere's 3D view.
When the user moves their head, the 3D scene shifts perspective as if looking through a window.

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────┐
│                     Abstract Interface                       │
│                                                             │
│  cwAbstractHeadTracker (QObject)                            │
│    ├─ signal: eyePositionChanged(QVector3D eyePos)          │
│    ├─ signal: headRotationChanged(QQuaternion rotation)     │
│    ├─ Q_PROPERTY: running (bool)                            │
│    ├─ Q_PROPERTY: available (bool)                          │
│    ├─ Q_PROPERTY: smoothing (double, 0.0-1.0)              │
│    ├─ slot: start()                                         │
│    └─ slot: stop()                                          │
│                                                             │
│  cwDlibHeadTracker : cwAbstractHeadTracker  ← Checkpoint 1  │
│    ├─ Uses OpenCV VideoCapture + dlib face landmarks        │
│    ├─ cv::solvePnP for 6-DOF pose estimation                │
│    └─ Runs detection on QThread worker                      │
│                                                             │
│  (Future: cwVisionHeadTracker using Apple Vision framework) │
│  (Future: cwMediaPipeHeadTracker)                           │
└─────────────────────────────────────────────────────────────┘
          │
          │ eyePos (QVector3D in meters, screen-relative)
          ▼
┌─────────────────────────────────────────────────────────────┐
│  cwHeadCoupledPerspectiveProjection         ← Checkpoint 2  │
│  (extends cwAbstractProjection)                             │
│    ├─ Receives eye position from tracker                    │
│    ├─ Computes asymmetric frustum via off-axis projection   │
│    ├─ Overrides calculateProjection()                       │
│    ├─ Delegates to cwPerspectiveProjection for base FOV     │
│    └─ Applies head offset to view matrix (see Options)      │
└─────────────────────────────────────────────────────────────┘
          │
          │ cwCamera::setProjection() + setViewMatrix()
          ▼
┌─────────────────────────────────────────────────────────────┐
│  Existing Render Pipeline (no changes needed)               │
│    cwCamera → cwScene → cwRhiScene → GPU                    │
└─────────────────────────────────────────────────────────────┘
```

## Key Integration Points (existing code)

| Class | File | Role |
|-------|------|------|
| `cwProjection::setFrustum()` | `cavewherelib/src/cwProjection.cpp:27` | Creates asymmetric frustum matrix |
| `cwCamera::setProjection()` | `cavewherelib/src/cwCamera.cpp:84` | Applies projection, emits signal |
| `cwCamera::setViewMatrix()` | `cavewherelib/src/cwCamera.cpp:93` | Applies view matrix, emits signal |
| `cwAbstractProjection` | `cavewherelib/src/cwAbstractProjection.h` | Base class — head-coupled projection subclasses this |
| `cwPerspectiveProjection` | `cavewherelib/src/cwPerspectiveProjection.h` | Provides base FOV/near/far; head-coupled delegates to it |
| `cwBaseTurnTableInteraction` | `cavewherelib/src/cwBaseTurnTableInteraction.cpp` | Builds view matrix from azimuth/pitch quaternion |
| `cw3dRegionViewer` | `cavewherelib/src/cw3dRegionViewer.cpp:29` | Owns ortho + perspective projection objects |
| `cwRootData` | `cavewherelib/src/cwRootData.h` | Central singleton, exposes subsystems to QML |
| `cwSettings` | `cavewherelib/src/cwSettings.h` | Per-user settings (add head tracking toggle + screen size) |
| `Info.plist.in` | `Info.plist.in` | macOS app bundle plist — needs `NSCameraUsageDescription` |
| `conanfile.py` | `conanfile.py` | Dependency manifest — add OpenCV + dlib |

## Off-Axis Projection Math

Given eye position `(ex, ey, ez)` in meters relative to screen center, and screen half-dimensions `(sw, sh)`:

```
scale = nearPlane / ez       (ez must be > 0; guard against zero)

left   = (-sw - ex) * scale
right  = ( sw - ex) * scale
bottom = (-sh - ey) * scale
top    = ( sh - ey) * scale

projection.setFrustum(left, right, bottom, top, nearPlane, farPlane)
viewMatrix = headOffset * turntableViewMatrix
```

When the user is centered (`ex=0, ey=0`), this reduces to a standard symmetric frustum.
The `ez` (distance from screen) controls FOV — closer = wider FOV, farther = narrower.

Note: `setFrustum()` sets the projection type to `PerspectiveFrustum`, which is already
handled in `cwBaseTurnTableInteraction::zoomLastPosition()` alongside the `Perspective` type.

### PerspectiveFrustum Compatibility Audit

All existing `switch(projection().type())` sites already handle `PerspectiveFrustum` alongside
`Perspective` — zoom, pan, capture, and export all work correctly. However, there is one issue:

**`zoomTo()` reads `fieldOfView()` and `aspectRatio()` (line 648-649):** These fields are only
populated by `setPerspective()`. When the projection uses `setFrustum()` (type = `PerspectiveFrustum`),
both return 0.0, producing a broken zoom distance.

**Fix:** `cwHeadCoupledPerspectiveProjection::calculateProjection()` should compute and store
FOV and aspect ratio on the `cwProjection` after calling `setFrustum()`:

```cpp
cwProjection proj;
proj.setFrustum(left, right, bottom, top, nearPlane, farPlane);
// Derive effective FOV from frustum geometry so zoomTo() works:
//   verticalFOV = 2 * atan((top - bottom) / (2 * nearPlane)) * 180/pi
//   aspectRatio = (right - left) / (top - bottom)
// Store via a new setter or by calling setPerspective() first then overriding
// with setFrustum() (setPerspective internally calls setFrustum, then sets
// FieldOfView/AspectRatio, then resets Type to Perspective — so order matters).
```

Alternatively, add `setFieldOfView()` and `setAspectRatio()` setters to `cwProjection` so they
can be set independently of the projection type. This is the cleaner approach since the values
are derived from the frustum geometry, not from user input.

Reference: Robert Kooima, "Generalized Perspective Projection" (2009).

## View Matrix Composition — cwViewMatrixComposer (Option B)

The turntable interaction and head tracker both need to influence the view matrix without knowing about each other. We use an intermediary composer to keep them decoupled.

### cwViewMatrixComposer

New object that sits between the turntable/head tracker and the camera. Neither the turntable nor the head tracker knows about the other.

```cpp
class CAVEWHERE_LIB_EXPORT cwViewMatrixComposer : public QObject
{
    Q_OBJECT

    Q_PROPERTY(cwCamera* camera READ camera WRITE setCamera NOTIFY cameraChanged)

public:
    explicit cwViewMatrixComposer(QObject* parent = nullptr);

    void setBaseViewMatrix(const QMatrix4x4& matrix);   // called by turntable
    void setHeadOffset(const QMatrix4x4& offset);        // called by head-coupled projection

    QMatrix4x4 baseViewMatrix() const;

private:
    void compose();  // pushes headOffset * baseView → camera->setViewMatrix()

    cwCamera* m_camera = nullptr;
    QMatrix4x4 m_baseViewMatrix;
    QMatrix4x4 m_headOffset;  // identity when head tracking is off
};
```

### Wiring in cw3dRegionViewer

- `cw3dRegionViewer` creates the composer and gives it the camera
- `cwBaseTurnTableInteraction` calls `composer->setBaseViewMatrix()` instead of `camera->setViewMatrix()` directly
- `cwHeadCoupledPerspectiveProjection` calls `composer->setHeadOffset()` when eye position changes
- When head tracking is disabled, `setHeadOffset(QMatrix4x4())` resets to identity — turntable works exactly as before
- Zero changes to `cwCamera`

#### Turntable `Camera->setViewMatrix()` Call Sites (9 total)

All 9 call sites in `cwBaseTurnTableInteraction.cpp` must be redirected to the composer:

| Line | Method | Purpose |
|------|--------|---------|
| 76 | `centerOn()` | Centers camera on a point (non-animated path) |
| 140 | `updateViewMatrixFromAnimation()` | Applies animated view matrix from `cwMatrix4x4Animation` |
| 222 | `resetView()` | Resets to default orientation and position |
| 325 | `translateLastPosition()` | Panning — translates by intersection delta |
| 397 | `updateRotationMatrix()` | Orbit rotation — applies azimuth/pitch quaternion |
| 456 | `zoomPerspective()` | Perspective zoom — translates along camera ray |
| 486 | `zoomOrtho()` | Orthographic zoom — translates to maintain point under cursor |
| 603 | `zoomTo()` | Recenters on bounding box |
| 679 | `zoomTo()` | Moves camera back along forward axis (perspective path) |

**Implementation approach:** `cwBaseTurnTableInteraction` gains a `cwViewMatrixComposer*` property (set by `GLTerrainRenderer.qml` or `cw3dRegionViewer`). Internally, a helper method replaces direct `Camera->setViewMatrix()` calls:

```cpp
void cwBaseTurnTableInteraction::setViewMatrix(const QMatrix4x4& matrix)
{
    if (m_viewMatrixComposer) {
        m_viewMatrixComposer->setBaseViewMatrix(matrix);
    } else {
        Camera->setViewMatrix(matrix);
    }
}
```

This keeps the turntable working identically when no composer is set (e.g., in tests or other viewers).

### New File

| File | Purpose |
|------|---------|
| `cavewherelib/src/cwViewMatrixComposer.h` | View matrix composition (turntable + head offset) |
| `cavewherelib/src/cwViewMatrixComposer.cpp` | Implementation |

---

## Checkpoint 1: Head Tracker Backend (OpenCV + dlib)

### Goal
Abstract tracker interface + OpenCV/dlib implementation that outputs smoothed 3D eye position at ~30 FPS.

### Dependencies

Add to `conanfile.py`:
```python
requires = [
    ...
    ("opencv/4.10.0"),   # Camera capture + solvePnP
    ("dlib/19.24.6"),     # Face landmark detection (68-point model)
]
```

dlib is available on Conan Center but pulls transitive deps (BLAS, LAPACK, etc.). Since we only need HOG face detection + shape predictor (no deep learning), consider using dlib in header-only mode without BLAS to reduce build-time impact.

#### dlib Shape Predictor Model

The 68-point face landmark model (`shape_predictor_68_face_landmarks.dat`, ~100 MB) is bundled with the application. The model is too large for git — download it during the CMake configure step:

```cmake
# Download dlib shape predictor model at configure time
set(DLIB_MODEL_URL "http://dlib.net/files/shape_predictor_68_face_landmarks.dat.bz2")
set(DLIB_MODEL_DIR "${CMAKE_BINARY_DIR}/dlib_models")
set(DLIB_MODEL_FILE "${DLIB_MODEL_DIR}/shape_predictor_68_face_landmarks.dat")

if(NOT EXISTS "${DLIB_MODEL_FILE}")
    message(STATUS "Downloading dlib shape predictor model...")
    file(DOWNLOAD "${DLIB_MODEL_URL}" "${DLIB_MODEL_DIR}/shape_predictor_68_face_landmarks.dat.bz2"
         EXPECTED_HASH SHA256=<fill-in-actual-hash-before-implementation>
         SHOW_PROGRESS)
    # Decompress .bz2 using CMake's built-in tar (portable, no dependency on bunzip2)
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E tar xjf
            "${DLIB_MODEL_DIR}/shape_predictor_68_face_landmarks.dat.bz2"
        WORKING_DIRECTORY "${DLIB_MODEL_DIR}")
endif()

# Install into app bundle
install(FILES "${DLIB_MODEL_FILE}" DESTINATION "${CMAKE_INSTALL_DATADIR}/models")
# macOS: copy into bundle Resources/
if(APPLE)
    set_source_files_properties("${DLIB_MODEL_FILE}" PROPERTIES
        MACOSX_PACKAGE_LOCATION Resources/models)
endif()
```

Use dlib's HOG face detector (faster on CPU than CNN, sufficient accuracy for head tracking).

### Dependency Spike — DONE

OpenCV 4.10.0 and dlib 19.24.2 integrate cleanly with the Conan + CMake build.
dlib 19.24.6 was not available on Conan Center; 19.24.2 was used instead.
sqlite3 required a version bump to >=3.48.0 (override) to resolve a transitive conflict.

### New Files — DONE

| File | Purpose |
|------|---------|
| `cavewherelib/src/cwAbstractHeadTracker.h/cpp` | Abstract interface: EMA smoothing, start/stop lifecycle, Q_PROPERTYs for QML |
| `cavewherelib/src/cwDlibHeadTracker.h/cpp` | Concrete tracker: macOS camera permissions, face-lost grace period + animation, worker thread management |
| `cavewherelib/src/cwDlibHeadTrackerWorker.h/cpp` | Worker thread: OpenCV VideoCapture + dlib HOG + shape predictor + solvePnP. All OpenCV/dlib types hidden behind pimpl |
| `testcases/HeadTrackerTest.cpp` | 6 test cases (31 assertions): initial state, lifecycle, smoothing, EMA filter, signals, dedup |

### cwAbstractHeadTracker Interface

```cpp
class CAVEWHERE_LIB_EXPORT cwAbstractHeadTracker : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool running READ isRunning NOTIFY runningChanged)
    Q_PROPERTY(bool available READ isAvailable NOTIFY availableChanged)
    Q_PROPERTY(double smoothing READ smoothing WRITE setSmoothing NOTIFY smoothingChanged)
    Q_PROPERTY(QVector3D eyePosition READ eyePosition NOTIFY eyePositionChanged)
    Q_PROPERTY(QQuaternion headRotation READ headRotation NOTIFY headRotationChanged)

public:
    explicit cwAbstractHeadTracker(QObject* parent = nullptr);
    virtual ~cwAbstractHeadTracker();

    bool isRunning() const;
    virtual bool isAvailable() const = 0;

    double smoothing() const;          // 0.0 = no smoothing, 1.0 = max
    void setSmoothing(double value);

    QVector3D eyePosition() const;     // meters, screen-relative
    QQuaternion headRotation() const;

public slots:
    void start();
    void stop();

signals:
    void runningChanged();
    void availableChanged();
    void smoothingChanged();
    void eyePositionChanged(QVector3D position);
    void headRotationChanged(QQuaternion rotation);
    void trackingLost();  // emitted after grace period with no face detection
    void errorOccurred(QString message);

protected:
    // Subclasses call this with raw (unsmoothed) values
    void setRawEyePosition(QVector3D position);
    void setRawHeadRotation(QQuaternion rotation);

    virtual void startTracking() = 0;
    virtual void stopTracking() = 0;

private:
    // Exponential moving average filter
    QVector3D applySmoothing(QVector3D raw, QVector3D previous) const;
};
```

### cwDlibHeadTracker Implementation

```
Processing pipeline (runs on worker QThread):

1. OpenCV VideoCapture grabs frame from webcam (camera index 0)
2. Convert to grayscale
3. dlib HOG face detector finds face bounding box
   - Run detector every 5th frame; track landmarks in between for speed
4. dlib shape predictor extracts 68 face landmarks
5. Select 6 key points for PnP: nose tip, chin, left/right eye corners,
   left/right mouth corners
6. Define corresponding 3D model points (generic face model in mm)
7. cv::solvePnP() → rotation vector + translation vector
8. Convert translation to meters, map to screen-relative coordinates:
   - x: positive = right of screen center
   - y: positive = above screen center (flip from OpenCV Y-down to CaveWhere Y-up)
   - z: distance from screen (always positive)
9. Emit raw position; base class applies smoothing
```

Frame rate throttling: The worker thread should process only the most recent frame from the webcam. If detection takes longer than the frame interval, stale frames are dropped rather than queued. Use a single-slot buffer: the capture thread writes the latest frame, the processing thread reads it. This decouples the tracker frame rate from the render frame rate.

#### Thread-Safe Frame Buffer Options

Since this is a single-producer (capture thread) / single-consumer (processing thread) scenario:

| Option | Mechanism | Pros | Cons |
|--------|-----------|------|------|
| **A. `QMutex` + swap** | Lock a `QMutex`, swap the `cv::Mat` into a shared slot, unlock. Consumer locks, moves it out. | Simple, familiar Qt pattern. | Lock contention if capture is fast. |
| **B. Triple buffer** | Three `cv::Mat` slots + two `std::atomic<int>` indices (write/read). Writer cycles through slots; reader picks the most recently completed slot. | Lock-free, zero contention. | More complex; 3x memory for frames. |
| **C. `std::atomic<std::shared_ptr<cv::Mat>>`** | Producer atomically stores a new `shared_ptr`; consumer atomically loads it. | Lock-free, simple API. | Requires C++20 `atomic<shared_ptr>` or `std::atomic_load/store` overloads for `shared_ptr` (C++11, deprecated C++20). |

**Recommendation:** Option A (`QMutex` + swap). The lock is held only for a pointer swap (nanoseconds), so contention is negligible at 30 FPS. It's the simplest to implement and debug, and uses standard Qt primitives consistent with the rest of the codebase. If profiling shows contention, upgrade to Option B.

Camera calibration: Use an approximate intrinsic matrix based on the webcam resolution and typical MacBook FOV (~60 degrees). Use `QCameraDevice::photoResolutions()` to get the actual capture resolution for a more accurate focal length estimate. Allow user override via settings.

#### Face-Lost Behavior

When the tracker loses the face (no detection for N consecutive frames):
1. Hold the last known eye position for a short grace period (~0.5 seconds) to handle momentary detection drops.
2. After the grace period, animate the eye position back to the default center position `(0, 0, defaultDistance)` over ~1 second using the same EMA smoothing filter (or a lerp with a fixed time constant).
3. This produces a gentle drift back to a standard symmetric frustum rather than an abrupt snap.

The tracker should emit a `trackingLost()` signal after the grace period. The `cwHeadCoupledPerspectiveProjection` drives the animation by interpolating `m_lastEyePos` toward center on a `QTimer` tick until it arrives (or a new face detection resumes).

### Calibration

Head tracking needs to know where the camera is relative to the screen center to produce
accurate screen-relative eye coordinates. Without calibration, the tracker assumes the
camera is at top-center of the screen, which is wrong for external monitors, offset webcams,
or when the user isn't centered.

#### Calibration Approach: Two-Point Gaze Calibration

The user looks at two known screen positions while the tracker records the corresponding
face landmark positions. This establishes the mapping from camera-space to screen-space.

```
Calibration flow:
1. Show a dot at screen center → user looks at it → record face position P_center
2. Show a dot at a known offset (e.g., top-right corner) → user looks at it → record P_corner
3. From P_center and P_corner, derive:
   - Camera offset: The face position when looking at center IS the camera-to-screen-center vector
   - Scale factor: The pixel displacement vs. face displacement ratio gives the
     mapping from camera-space translation to screen-space translation
```

This corrects for:
- **Camera position:** laptop cameras are above the screen, external webcams could be anywhere
- **Screen distance:** establishes the baseline `ez` (distance from screen)
- **Individual face geometry:** different people have different interpupillary distances, nose sizes, etc., which affect solvePnP output

#### Stored Calibration Data

```cpp
struct CalibrationData {
    QVector3D cameraOffset;     // camera position relative to screen center (meters)
    double    scaleX = 1.0;     // horizontal mapping scale
    double    scaleY = 1.0;     // vertical mapping scale
    double    baselineZ = 0.5;  // baseline distance from screen (meters)
};
```

Persisted via `QSettings`. The tracker applies the calibration as a post-processing step
on the raw solvePnP output before emitting `eyePositionChanged()`:

```cpp
QVector3D calibrate(QVector3D rawEyePos) const {
    return QVector3D(
        (rawEyePos.x() - m_calibration.cameraOffset.x()) * m_calibration.scaleX,
        (rawEyePos.y() - m_calibration.cameraOffset.y()) * m_calibration.scaleY,
        rawEyePos.z()  // z is distance from camera, already correct
    );
}
```

#### Calibration Timing

- **Checkpoint 1-2:** Use a simple default assumption (camera at top-center, 0.5m baseline distance). This is good enough to validate the parallax effect.
- **Checkpoint 3:** Add the full two-point calibration UI flow with a "Calibrate" button in the Head Tracking settings panel.

### macOS Camera Permissions

Add to `Info.plist.in`:
```xml
<key>NSCameraUsageDescription</key>
<string>CaveWhere uses the camera for head tracking to create an immersive 3D parallax effect.</string>
```

At runtime, before starting the tracker:
```cpp
QCameraPermission cameraPermission;
switch (qApp->checkPermission(cameraPermission)) {
case Qt::PermissionStatus::Granted:
    startTracking();
    break;
case Qt::PermissionStatus::Undetermined:
    qApp->requestPermission(cameraPermission, this, &cwDlibHeadTracker::onPermissionResult);
    break;
case Qt::PermissionStatus::Denied:
    emit errorOccurred("Camera permission denied");
    break;
}
```

### Tests

| Test | Description |
|------|-------------|
| `cwAbstractHeadTracker` smoothing | Verify EMA filter produces expected output for known input sequence |
| `cwDlibHeadTracker::isAvailable()` | Returns false when no camera present (CI) |
| Mock tracker subclass | Feeds synthetic positions, verifies signals emit correctly |
| solvePnP coordinate mapping | Given known 2D landmark pixel positions, verify the output 3D eye position is correct (covers the OpenCV Y-down → CaveWhere Y-up flip and camera-relative → screen-relative transform) |
| Start/stop lifecycle | Rapidly start/stop tracker multiple times, verify no deadlocks, dangling threads, or use-after-free (run under ASAN/TSAN) |
| Face-lost animation | When mock tracker stops emitting positions, verify eye position animates back to center after grace period |

### Acceptance Criteria
- [x] `cwAbstractHeadTracker` compiles and passes unit tests for smoothing logic
- [ ] `cwDlibHeadTracker` opens webcam, detects face, emits `eyePositionChanged` at >= 15 FPS
- [x] Camera permission flow works on macOS (prompt appears, denied state handled)
- [ ] Tracker can be started/stopped without leaks (verify with ASAN)
- [x] Backend is fully encapsulated — no OpenCV/dlib headers leak into the abstract interface
- [x] Stale frames are dropped, not queued (frame rate throttling)
- [x] Face-lost gracefully animates back to center position

---

## Checkpoint 2: Head-Coupled Projection + QML Integration

### Goal
Wire the tracker output to the camera projection so moving your head shifts the 3D view.
Expose toggle and settings in the UI.

### New Files

| File | Purpose |
|------|---------|
| `cavewherelib/src/cwHeadCoupledPerspectiveProjection.h` | Subclass of `cwAbstractProjection`; computes off-axis frustum from eye position |
| `cavewherelib/src/cwHeadCoupledPerspectiveProjection.cpp` | Projection math + camera integration |
| `cavewherelib/src/cwViewMatrixComposer.h` | View matrix composition (turntable + head offset) |
| `cavewherelib/src/cwViewMatrixComposer.cpp` | Implementation |
| `cavewherelib/qml/HeadTrackingSettings.qml` | UI toggle + settings panel |

### cwHeadCoupledPerspectiveProjection

Subclasses `cwAbstractProjection` so it plugs into the existing projection system on `cw3dRegionViewer`. It replaces `cwPerspectiveProjection` as the active projection when head tracking is enabled.

```cpp
class CAVEWHERE_LIB_EXPORT cwHeadCoupledPerspectiveProjection : public cwAbstractProjection
{
    Q_OBJECT
    QML_NAMED_ELEMENT(HeadCoupledPerspectiveProjection)

    Q_PROPERTY(cwAbstractHeadTracker* tracker READ tracker WRITE setTracker NOTIFY trackerChanged)
    Q_PROPERTY(double fieldOfView READ fieldOfView WRITE setFieldOfView NOTIFY fieldOfViewChanged)
    Q_PROPERTY(double screenWidthMeters READ screenWidthMeters WRITE setScreenWidthMeters NOTIFY screenWidthMetersChanged)
    Q_PROPERTY(double screenHeightMeters READ screenHeightMeters WRITE setScreenHeightMeters NOTIFY screenHeightMetersChanged)
    Q_PROPERTY(double sensitivity READ sensitivity WRITE setSensitivity NOTIFY sensitivityChanged)

protected:
    cwProjection calculateProjection() override;

    // Note: also add `override` to cwPerspectiveProjection::calculateProjection()

private slots:
    void onEyePositionChanged(QVector3D eyePos);

private:
    cwAbstractHeadTracker* m_tracker = nullptr;
    double m_fieldOfView = 55.0;
    double m_screenWidthMeters = 0.30;
    double m_screenHeightMeters = 0.19;
    double m_sensitivity = 1.0;
    QVector3D m_lastEyePos;
};
```

Key behaviors:
- `calculateProjection()` computes the asymmetric frustum based on `m_lastEyePos` + `m_fieldOfView` + screen dimensions. This is called automatically by `cwAbstractProjection::updateProjection()` on resize.
- `onEyePositionChanged()` stores the new eye position and calls `updateProjection()`.
- `fieldOfView` property allows the FOV slider in `CameraProjectionSettings.qml` to keep working — it controls the base FOV that the asymmetric frustum is derived from.
- When the tracker reports no face (or the eye position is centered), the frustum degenerates to a standard symmetric perspective — seamless fallback.

### Integration with cw3dRegionViewer

`cw3dRegionViewer` already owns `cwOrthogonalProjection` and `cwPerspectiveProjection` and switches between them. Add a third:

```cpp
// cw3dRegionViewer constructor:
HeadCoupledProjection = new cwHeadCoupledPerspectiveProjection(this);
HeadCoupledProjection->setViewer(this);
HeadCoupledProjection->setEnabled(false);  // off by default
```

When the user enables head tracking:
1. If orthographic mode is active, switch to perspective first (disable `cwOrthogonalProjection`)
2. Disable `cwPerspectiveProjection`
3. Copy FOV/near/far from `cwPerspectiveProjection` to `cwHeadCoupledPerspectiveProjection`
4. Enable `cwHeadCoupledPerspectiveProjection`
5. Start the tracker
6. Disable the ortho/perspective toggle in the UI

When disabled, reverse the process. Since `cwAbstractProjection::setEnabled()` calls `updateProjection()` which applies the projection to the camera, switching is handled by the existing mechanism.

### View Matrix Composition

Uses `cwViewMatrixComposer` (see above). The head-coupled projection holds a pointer to the composer:
- `onEyePositionChanged()` calls `composer->setHeadOffset(offset)` AND calls `updateProjection()` for the frustum
- The composer composes `headOffset * baseViewMatrix` and pushes to the camera
- When head tracking is disabled, head offset resets to identity

### QML UI

Add a "Head Tracking" checkbox to `CameraOptionsTab.qml` inside the existing `ColumnLayout`, after the Projection group box. When checked, it auto-switches to perspective mode (disabling ortho) and enables the head-coupled projection + tracker.

Note: `CameraOptionsTab.qml` receives a `GLTerrainRenderer` as `viewer`. Its `renderer` alias resolves to the `RegionViewer` (`cw3dRegionViewer`) instance. The new `headCoupledProjection` and `headTracker` Q_PROPERTYs must be added to `cw3dRegionViewer` (alongside the existing `orthoProjection` and `perspectiveProjection` properties).

```qml
// In CameraOptionsTab.qml, add after the Projection QC.GroupBox:
QC.GroupBox {
    title: "Head Tracking"
    Layout.fillWidth: true

    HeadTrackingSettings {
        headProjection: itemId.viewer.renderer.headCoupledProjection
        headTracker: itemId.viewer.renderer.headTracker
    }
}
```

```qml
// HeadTrackingSettings.qml
ColumnLayout {
    required property HeadCoupledPerspectiveProjection headProjection
    required property AbstractHeadTracker headTracker

    QC.CheckBox {
        text: "Enable"
        checked: headProjection.enabled
        onToggled: headProjection.enabled = checked
    }

    QC.Label {
        text: "Smoothing"
        font.pixelSize: Theme.fontSizeBody
        visible: headProjection.enabled
    }
    QC.Slider {
        from: 0.0; to: 1.0
        value: headTracker.smoothing
        onMoved: headTracker.smoothing = value
        visible: headProjection.enabled
    }

    QC.Label {
        text: "Sensitivity"
        font.pixelSize: Theme.fontSizeBody
        visible: headProjection.enabled
    }
    QC.Slider {
        from: 0.1; to: 3.0
        value: headProjection.sensitivity
        onMoved: headProjection.sensitivity = value
        visible: headProjection.enabled
    }
}
```

### Orthographic Mode Interaction

When the user enables head tracking:
- Auto-switch from orthographic to perspective (head tracking requires perspective)
- Disable the ortho/perspective toggle in `CameraProjectionSettings.qml` while head tracking is active
- When head tracking is disabled, the user can freely switch back to ortho if desired

### Ownership + Lifecycle

- `cw3dRegionViewer` owns the `cwHeadCoupledPerspectiveProjection` (consistent with how it owns the other projection objects)
- `cw3dRegionViewer` also owns the `cwAbstractHeadTracker` instance
- Tracker is created lazily on first enable (avoid loading dlib model until needed)
- Tracker stops when the 3D view is not visible or when the app is minimized
- Settings (enabled, smoothing, sensitivity, screen dimensions) persist via `QSettings`

### Screen Size Detection

Attempt auto-detection from `QScreen::physicalSize()`:
```cpp
QScreen* screen = QGuiApplication::primaryScreen();
QSizeF physicalSize = screen->physicalSize(); // millimeters
m_screenWidthMeters = physicalSize.width() / 1000.0;
m_screenHeightMeters = physicalSize.height() / 1000.0;
```

Allow manual override in settings (some screens report incorrect physical size).

### Tests

| Test | Description |
|------|-------------|
| Off-axis math | Verify frustum params for known eye positions (center, left, right, close, far) |
| Centered eye = symmetric | When eye at (0, 0, 0.5m), frustum should match standard `setPerspective()` with same FOV |
| Enable/disable | Toggling restores original projection exactly |
| Resize while tracking | Verify `calculateProjection()` re-derives correct asymmetric frustum on aspect ratio change |
| Composition with turntable | Verify head offset + turntable orbit produce correct combined view matrix |
| Projection coexistence | Verify that enabling head-coupled projection disables ortho and perspective; only one projection is active at a time |
| Composer without head tracking | Verify composer with identity head offset passes turntable view matrix through unchanged |

### Acceptance Criteria
- [ ] Moving head left/right shifts the 3D view horizontally (parallax effect)
- [ ] Moving head up/down shifts vertically
- [ ] Moving head closer/farther changes apparent FOV
- [ ] FOV slider still works with head tracking enabled
- [ ] Turntable orbit/pan/zoom work normally with head tracking enabled
- [ ] Toggle on/off is seamless (no jump in view)
- [ ] Smoothing slider visibly reduces jitter
- [ ] Sensitivity slider scales the effect magnitude
- [ ] Settings persist across app restarts
- [ ] No performance regression when head tracking is disabled

---

## Checkpoint 3: Polish + Platform Support

### Goal
Production-quality experience: calibration, fallback backends, cross-platform.

### Tasks
- [ ] Add two-point gaze calibration UI (see Calibration section above)
- [ ] Add status indicator in 3D view (small icon showing tracking state)
- [ ] Windows support: verify OpenCV camera access, no `NSCameraUsageDescription` needed
- [ ] Linux support: verify V4L2 camera access
- [ ] Evaluate Apple Vision backend for macOS (better perf, uses Neural Engine) — behind `#ifdef Q_OS_MACOS`
- [ ] Profile CPU usage and optimize if needed
- [ ] Verify bundled dlib shape predictor model loads correctly on all platforms
- [ ] Add to user documentation / help

---

## Resolved Questions

1. ~~**dlib model distribution**~~: **Decided** — bundle the 100 MB shape predictor in the app. Downloaded via CMake at configure time (not stored in git).

2. ~~**Camera selection**~~: **Decided** — use the default FaceTime camera (camera index 0). No camera selection UI. This is prototype code.

3. ~~**Multiple monitors**~~: **Decided** — not addressing for now. The effect won't be as good if you're not looking at the camera anyway.

4. ~~**Performance budget**~~: **Decided** — one full core at 100% is acceptable. This is prototype code to see if the effect works. Optimization deferred to Checkpoint 3.

5. ~~**Interaction with orthographic mode**~~: **Decided** — auto-switch to perspective when enabling head tracking. Disable the ortho/perspective toggle while head tracking is active. Re-enable when head tracking is turned off.

6. ~~**View matrix composition**~~: **Decided** — Option B, intermediary `cwViewMatrixComposer`. Zero changes to `cwCamera`, minimal change to turntable (swap target from camera to composer).
