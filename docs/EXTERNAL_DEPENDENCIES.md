# External Dependencies

The pipeline uses three collaborator repositories as Git submodules. They are
kept untouched so that this repository contains only the project-owned adapter,
ROS 2 processing, launch files, and documentation.

## Reproduce the checkout

Clone with submodules, or initialize them after a normal clone:

```bash
git clone --recurse-submodules https://github.com/jfcoeur/FBGS-ROS2-pipeline.git
cd FBGS-ROS2-pipeline
git submodule update --init --recursive
```

The submodules are checked out at the exact commits recorded by the parent
repository. Verify them with:

```bash
git submodule status
```

The current dependency locations and upstream repositories are:

| Local path | Upstream |
| --- | --- |
| `external dependencies/ws_smartneedle` | `https://github.com/SmartNeedle/ws_smartneedle.git` |
| `external dependencies/SmartNeedleIGTL-3DSlicer` | `https://github.com/SmartNeedle/SmartNeedleIGTL-3DSlicer.git` |
| `external dependencies/OpenIGTLink` | `https://github.com/openigtlink/OpenIGTLink.git` |

## How they are used

- `OpenIGTLink` is built locally and supplied to the ROS 2 C++ build through `OpenIGTLink_DIR`.
- `ws_smartneedle/src/ros2_igtl_bridge` supplies the untouched OpenIGTLink ROS bridge package.
- `ws_smartneedle/src/smartneedle_interface` supplies the collaborator message and interface package used by the project-owned processing nodes.
- `SmartNeedleIGTL-3DSlicer/SmartNeedle` is added to the Slicer module paths.
- `ros2_smartneedle_adapter` is project-owned and publishes the collaborator OpenIGTLink topics at 100 Hz without modifying the external bridge.

## Updating intentionally

Do not edit files inside a submodule as part of normal pipeline development.
To adopt a newer collaborator revision, update the submodule in its directory,
return to the parent repository, inspect the resulting gitlink change, and
commit that change:

```bash
git -C "external dependencies/ws_smartneedle" fetch origin
git -C "external dependencies/ws_smartneedle" checkout <reviewed-commit>
git add .gitmodules "external dependencies/ws_smartneedle"
git commit -m "Update ws_smartneedle dependency"
```

Repeat the same pattern for the other submodules only after reviewing their
compatibility with the current ROS and Slicer contracts.
