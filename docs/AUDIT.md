# Second Audit (2026-09-21)

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

- Physical interrogator and Slicer GUI tests require the user's Linux/hardware
  setup. Spectra framing was checked against a private ShapeCore capture;
  synthetic regression tests cover the corrected convention without distributing it.
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
