# External Dependencies

All external source remains untouched. The parent repository records exact
commit gitlinks; .gitmodules records URLs.

| Directory under external dependencies | Upstream | Pinned revision |
| --- | --- | --- |
| ws_smartneedle | https://github.com/SmartNeedle/ws_smartneedle.git | 82b27033b96fbe1b53dbdb62efb3c6d8cb0fa645 |
| SmartNeedleIGTL-3DSlicer | https://github.com/SmartNeedle/SmartNeedleIGTL-3DSlicer.git | d2e4f822d5f80f72c5e5062fc6968857eda74d1c |
| OpenIGTLink | https://github.com/openigtlink/OpenIGTLink.git | 94244fed7051e00cbcd4c341a7d7656be8ac404a |

## Initialize and synchronize

```bash
git pull --ff-only
bash scripts/init_dependencies.sh
git submodule status
```

The initialization script intentionally does not recurse. ws_smartneedle has
gitlinks for ros2_hyperion_interrogator and ros2_needle_shape_publisher without
a .gitmodules URL mapping. They are unused, and recursive initialization fails.
Do not fix this by editing the external repository.

## Use

OpenIGTLink supplies the native transport library. The ws_smartneedle repository
supplies ros2_igtl_bridge and its message definitions. Its smartneedle_interface
is retained as the original interface reference but is not built or launched:
our separate ros2_smartneedle_adapter implements that contract with a 100 Hz timer.
The local fbg_shape_msgs package defines the processing messages.

The pinned bridge's CMakeLists.txt installs a launch directory that is absent
from its checkout. The build helper injects scripts/cmake/bridge_install_compat.cmake
at CMake project initialization. It skips only that missing directory install;
all bridge sources and other install commands remain unchanged. The local
adapter supplies the launch files used by this pipeline.

Only the external bridge package is selected from ws_smartneedle; the old
interrogator, reconstruction, robot, virtual dataset, and 1 Hz interface paths
are not part of the build. Slicer loads SmartNeedleIGTL-3DSlicer/SmartNeedle.

The external smartneedle_interface package declares its license as TODO.
Obtain the collaborators' licensing permission before distributing their source;
this repository does not assign or change their licenses.

## Deliberate upgrades

Fetch an upstream dependency, inspect a chosen revision, and check it out inside
that submodule. Commit the resulting gitlink change in the parent only after
testing. Do not use submodule update --remote for routine synchronization:
it changes the tested dependency versions.

These pins reproduce the audited checkout; pinning does not mean newer upstream
commits have been tested or should be adopted automatically.
