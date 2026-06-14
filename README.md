# ros2_node_inspector

A lightweight C++ GUI that lists every live ROS 2 node and, on selecting one,
shows its direct connections (publishers/subscribers it talks to) in two
interchangeable views — a **table** and an **ego graph** — each connection
carrying a single status icon:

| Icon            | Meaning                                                        |
|-----------------|----------------------------------------------------------------|
| ✓ green tick    | healthy (type + QoS compatible, data flowing)                  |
| ? amber circle  | QoS incompatible (Request-vs-Offered violation)                |
| ✗ red cross     | type mismatch, or compatible but **dead** (0 Hz)               |
| ○ grey ring     | probe window not yet elapsed (liveness still measuring)        |

Click any status icon for a plain-language explanation of the fault, e.g.
*"/map: publisher node_a offers TRANSIENT_LOCAL, subscriber node_b requests
VOLATILE → durability incompatible"*.

Built with Dear ImGui (vendored) + GLFW + OpenGL3. Diagnoses a broken topic link
in seconds instead of polling the CLI.

## Build

This is a standard `ament_cmake` package. From the colcon workspace root
(`/home/ros/ws` inside the devcontainer):

```bash
colcon build --packages-select ros2_node_inspector --merge-install
source install/setup.bash
```

System build dependencies (already installed in the Docker images):
`libglfw3-dev`, `libgl1-mesa-dev`, `build-essential`, `cmake`, `git`.
Dear ImGui is vendored under `third_party/imgui` — no extra fetch step.

## Run

The GUI is an X11 app. On a **Linux host**, just launch it:

```bash
ros2 run ros2_node_inspector node_inspector
```

No `xhost +local:` is needed: `docker-compose.yml` bind-mounts your host's X
cookie (`$XAUTHORITY`, default `~/.Xauthority`) to `/tmp/.docker.xauth` and sets
`XAUTHORITY` to it. Because the container user is UID 1000 (matching the host)
and uses host networking, that cookie authenticates directly. If you start the
container with `docker compose` from a shell that lacks a valid `DISPLAY` /
Xauthority (e.g. over plain SSH), pass them through or fall back to
`xhost +local:`. A missing display shows `X11: Failed to open display` and the
app exits cleanly with code 1.

Pick a node from the dropdown; switch between **Table** and **Graph** with the
tab toggle.

### Controls (Graph view)

- **Drag empty canvas** — pan.
- **Mouse wheel** — zoom about the cursor.
- **Click a peer node box** — recenter the graph on that node.
- **Click a status icon** — open the fault popup (works in both views).

The dropdown shows per-node counts `✓N ?N ✗N` aggregating each node's
connections. **Counts cover type + QoS only** — liveness (dead/Hz) is shown only
for the *selected* node, because we never subscribe to unselected nodes.

## Try it

A ready-made demo graph (`demo/`) brings up three named pairs — one healthy and
two with deliberate QoS faults — so every status icon shows up at once:

```bash
ros2 launch ros2_node_inspector qos_demo.launch.py
# or straight from source, no install needed:
ros2 launch ./demo/qos_demo.launch.py
```

| Pair                    | Topic              | QoS                                   | Inspector shows           |
|-------------------------|--------------------|----------------------------------------|---------------------------|
| `talker_1`/`listener_1` | `/demo_ok`         | RELIABLE/VOLATILE both                 | ✓ green tick, live 2 Hz   |
| `talker_2`/`listener_2` | `/demo_reliability`| BEST_EFFORT pub vs RELIABLE sub        | ? reliability incompatible|
| `talker_3`/`listener_3` | `/demo_durability` | VOLATILE pub vs TRANSIENT_LOCAL sub    | ? durability incompatible |

Select e.g. `/talker_2` and click its status icon: *"/demo_reliability:
publisher /talker_2 offers BEST_EFFORT, subscriber /listener_2 requests RELIABLE
→ reliability incompatible"*. Ctrl-C in the launch terminal tears the graph down.

`demo/qos_demo_node.py` is a small parametrized rclpy node (role + topic + QoS
from parameters); the launch file just spawns six of them with distinct node
names. Tweak it to add type-mismatch or dead-link cases.

## Distro matrix

| Distro  | Role                  | Toolchain        |
|---------|-----------------------|------------------|
| Humble  | primary dev target    | Ubuntu 22.04 / GCC 11 |
| Jazzy   | compatibility gate    | Ubuntu 24.04 / GCC 13 |

The devcontainer attaches to **Humble**. Jazzy is build-verified
non-interactively (no daily-dev surface):

```bash
docker compose run --rm app-jazzy \
  colcon build --packages-select ros2_node_inspector --merge-install
```

The code uses C++20 but avoids `std::format` (lands in GCC 13) so it builds on
Humble's GCC 11.

### Other host OSes

macOS/Windows hosts need an X server (XQuartz / VcXsrv) and `DISPLAY` pointed at
it; the `docker-compose.yml` X11 mount assumes a Linux host. Software rendering
(mesa/llvmpipe) is sufficient — no GPU passthrough required.

## FontAwesome (optional)

Status icons are hand-drawn vector primitives and need no font. As an optional
upgrade, drop `fa-solid-900.ttf` into `third_party/fonts/` (git-ignored): if
present it is installed to the package share and merged into the ImGui atlas for
richer toolbar/legend glyphs. The app builds and runs identically without it.

The UI text font is the system **DejaVuSans** (for arrow/dash/bullet glyphs),
falling back to ImGui's built-in bitmap font if absent.
