# Pipeline Audit (2026-09-24)

Scope: all project-owned tracked source, configuration, launch/build scripts,
messages, documentation, and tools; top-level generated/backup directories;
external dependency inventory and the active bridge/interface/Slicer contracts.
External repositories remain pinned and unmodified, including unused upstream
files. They are dependencies, not project-owned cleanup targets.

## Findings addressed

- Adapter lacked setup.cfg, so ROS could not reliably locate its executable.
- Recursive submodule initialization failed on two URL-less legacy gitlinks.
- Unit-labelled meter calibration keys were silently interpreted as millimeters.
- Calibration could silently fall back to placeholders, accept bad indices,
  or mismatch positions and vectors.
- TCP shutdown could wait indefinitely for an idle sender. Acquisition is now
  interruptible; tcp_host/tcp_port changes switch sources with nodes running.
- Nonzero interrogator errors and nonfinite curvature/angle data no longer
  produce new reconstructions.
- The adapter stops after 0.5 seconds without valid shape input and logs recovery.
  Repeated output frames preserve input sequence identity.
- Simulator channel count derives from calibration. Timing uses monotonic
  deadlines and motion depends on simulated time, not publishing rate.
- Packet field/block lengths are checked; packet allocation has a 64 MiB cap.
- Reconstruction rejects invalid positions rather than silently deleting
  measurement entries and misaligning their curvature indices.
- Removed unused reconstruction helper and corrected millimeter fallback values.
- Fixed ignored input-topic launch argument, missing adapter install metadata,
  stale library/config paths, and obsolete documentation.
- Removed the unneeded external interface from the active build.
- Linux CI identified a missing upstream bridge launch directory. A local CMake
  hook skips that nonexistent directory install without editing the dependency.
- Bounded latency statistics and removed guessed source-clock interpretations.

## User-confirmed behavior

- Pause adapter output after 0.5 seconds of stale input.
- Switch real/simulated sources without restarting ROS nodes or Slicer.
- Suppress reconstruction for any nonzero interrogator error field.

## Cleanup

The recovery snapshot contained 977 files. Every non-bytecode file's Git blob
was found in parent or dependency Git history before deletion. The snapshot
and old Linux-generated OpenIGTLink build cache were removed.
Linux shell files are normalized to LF. External dependency contents were not
trimmed, renamed, or edited.

## Verification

Portable Python contract tests and shell syntax checks pass locally.
Linux ROS Humble verification passed on commit e46b015:
https://github.com/jfcoeur/FBGS-ROS2-pipeline/actions/runs/35641815604
All four ROS packages built. All six Python tests passed, including the
complete TCP-to-external-OpenIGTLink wire path, live source switching,
nonzero-error suppression, stale timeout, and recovery. Colcon reported
9 tests, 0 errors, 0 failures, and 0 skipped, covering parser bounds,
SE(3) against an independent matrix-exponential oracle, fragmented TCP,
source switching, and idle shutdown.

The GitHub workflow repeats these checks on future updates. These results
verify synthetic sources and the actual external transport, not hardware
calibration accuracy or Slicer GUI rendering.

## Remaining target validation and external limits

Subsequent user-approved reconstruction update: midpoint boundaries assign
each measurement to one constant-curvature segment, spanning the physical base
to the calibrated tip. The 19 output points are now segment boundaries, and
the represented length is 196.391633064447 mm. Regression references and the
end-to-end straight-needle expectation have been updated for this model.

- A functional physical-interrogator and Slicer GUI smoke test was completed
  on 2026-09-25; see the dated hardware section below. Quantitative known-bend
  accuracy, repeatability, and drift measurements remain unverified.
  Spectra framing was checked against a private ShapeCore capture; synthetic
  regression tests cover the corrected convention without distributing it.
- 100 Hz is a timer target; distinct input frame rate and Slicer rendering rate
  must be measured independently.
- The external Slicer module retains the previous geometry during stale input,
  uses CurveMaker for visual smoothing, and observes the STRING header separately
  from POINT updates. These messages are not atomic.
- A source switch can create a short gap or shape discontinuity; no blending or
  automatic simulation fallback is applied.
- Nonzero error codes are conservatively rejected; vendor code meanings remain
  unspecified. Packet source timestamps are preserved without guessing units.
- External smartneedle_interface contains a TODO license declaration. Confirm
  upstream distribution permissions with collaborators.

## Follow-up cleanup (2026-09-24)

- Removed the unused, separate shape-publisher node and its launch option. Shape
  reconstruction now has one supported route: the curvature callback publishes
  CurvatureFrame and PoseArray from the same input frame.
- Removed the curvature_scale ROS parameter and processing configuration. Raw
  interrogator curvature is used directly in 1/mm; the parser only accepts an
  optional legacy scale row when every value is 1, then ignores it.
- Removed duplicate sensor calibration values from pipeline.yaml and C++
  fallbacks. The selected sensor calibration file is the only source of sensor
  positions, first FBG index, angle convention, and needle length. Missing
  calibration now fails at startup rather than silently selecting a placeholder.
- Reject zero or negative first_fbg_index and averaging windows before converting
  to unsigned values, preventing invalid indices and accidental huge windows.
- Confirmed the 20-value simulator frame has the observed 15,767-byte size and
  field dimensions; synthetic content and constant-size timing remain an
  approximation of one capture, not a model of live interrogator jitter.
- Confirmed the three direct external submodules are clean at the pinned commits.
  Recursive status inspection reaches the two URL-less legacy gitlinks inside
  ws_smartneedle and stops as documented; those paths are not used by this build.

Behavior impact: the active calibrated launch route keeps the same curvature,
angle, reconstruction, and output calculations. Directly launching the C++ node
without its calibration file now fails safely. The old --separate-shape option
and shape_publisher_node executable were removed; users of that comparison route
must use the fused pipeline.

Verification: commit 1f066b5 passed the GitHub Ubuntu 22.04 / ROS Humble build,
all 7 Python tests, and all 12 colcon tests (0 errors, 0 failures, 0 skipped):
https://github.com/jfcoeur/FBGS-ROS2-pipeline/actions/runs/36055843077
On this Windows audit host, all 4 portable tests passed; 3 ROS-specific tests
were skipped because ROS 2 is unavailable here. Python syntax and whitespace
checks passed. The three direct external submodules have no tracked changes.

Retest scope: the required Linux rebuild and synthetic ROS/IGTL/Slicer retest
were completed on 2026-09-24. Functional real-interrogator rate, packet flow,
Slicer display, and live source switching were then checked on 2026-09-25; see
the dated hardware section below. Quantitative known-bend calibration validation,
repeatability, and drift measurements remain open.


## Latest Linux simulation verification (2026-09-24)

After pulling the audit update, the Linux ROS Humble build completed all four
packages. It emitted non-fatal unused-CMake-argument notices and the external
bridge's ROSIDL deprecation notices; there were no build failures. The subsequent
test-only update was pulled without rebuilding.

- The full Python suite passed: 7 tests, 0 failures/errors/skips. Its end-to-end
  synthetic TCP-to-OpenIGTLink check measured 100.0 Hz reconstructed shapes and
  100.0 Hz POINT output.
- Colcon passed: 12 tests, 0 errors, 0 failures, 0 skipped.
- Manual simulated operation through ROS, the OpenIGTLink bridge, and Slicer
  succeeded. Raw frames, curvature, reconstructed shape, and IGTL POINT topics
  each measured approximately 100 Hz.
- Stopping the simulator caused stale-input publication pause; restarting it
  resumed output and Slicer motion without restarting ROS or Slicer.
- The latency probe reported approximately 100.1 Hz curvature and shape rates.
  ROS receipt-to-shape-subscriber latency settled at 5.15 ms median and 7.17 ms
  p95, with an 88.22 ms maximum outlier. This metric excludes sensor acquisition
  and Slicer rendering.
- GitHub Actions passed the Linux Humble workflow on test commit bde8256:
  https://github.com/jfcoeur/FBGS-ROS2-pipeline/actions/runs/36063286446
- The Slicer dependency checkout showed only a modified tracked Python bytecode
  cache after module use. That one cache file was restored; its submodule status
  is now clean. No collaborator source files were changed.

This section records the 2026-09-24 simulated-path verification only. The
subsequent hardware smoke test is recorded below; neither simulation results nor
the functional hardware checks establish quantitative calibration accuracy,
repeatability, or drift.

## Linux hardware smoke test (2026-09-25)

User-run validation on the Linux target with the ShapeCore interrogator and
Slicer connected:

- Linux reached the Windows ShapeCore endpoint at 10.100.51.10:50012 over the
direct Ethernet link; the TCP connection succeeded.
- Raw FBG frames measured approximately 99.8 Hz. Curvature processing measured
approximately 97-98 Hz, reconstructed PoseArray approximately 97 Hz, and
OpenIGTLink POINT output approximately 100 Hz.
- The reconstructed shape contained 19 poses (base plus 18 segment boundaries).
A reported bent-shape tip was (-0.58, 2.73, 196.36) mm; the calibrated full
needle length is 196.391633 mm. The z coordinate is not itself the arc length.
- A zero interrogator error field was observed. The displayed shape appeared
stationary when held and followed a gentle physical bend.
- The real-to-simulation and simulation-to-real switches both succeeded while
ROS nodes and Slicer remained running. During simulation, input, reconstructed
shape, and OpenIGTLink output each measured approximately 100 Hz, and the
simulated needle moved in Slicer.
- During one hardware latency-probe interval, curvature and shape rates were
about 97.5 Hz; ROS receipt-to-shape-subscriber latency was 2.62 ms median,
4.06 ms p95, and 18.74 ms maximum. It excludes acquisition, transport before
ROS receipt, OpenIGTLink delivery, and Slicer rendering.

This was a functional smoke test, not quantitative calibration validation.
The visual stationary check and single bend do not establish drift, repeatability,
absolute shape accuracy, or sensor-to-robot registration. The observed sub-100 Hz
curvature/shape rates also warrant future investigation if a strict 100 Hz
hardware reconstruction rate is required. No external dependency source was
modified by this test.
