# Plan: ROS 2 Node Connection Inspector

> ## ⚠️ AGENT PROTOCOL — READ FIRST
> 1. **Attempt exactly ONE milestone per run** — the lowest-numbered one still marked `[ ]` below. Do not start the next.
> 2. A milestone is **DONE** only when: **(a)** the project compiles clean on Humble, **AND (b)** its gate passes — for milestones with GUI features, a **human manually runs it and confirms**; for non-GUI milestones, **unit tests pass**. Each milestone states its own gate.
> 3. When (and only when) the gate passes, **flip its box to `[x]` in the status board below** and stop. Report what to manually verify if a human gate is pending.
> 4. Never reverse a corrected design decision in this file on the basis of convenience. The "why" notes are load-bearing.
>
> ## Milestone status board
> - [x] **M0 — Project setup** · gate: devcontainer opens, image builds, Claude runs inside, first commit exists *(no GUI, no unit tests — gate is "container comes up")*
> - [x] **M1 — Backend & diagnostics** · gate: **unit tests pass** (non-GUI) *(28 gtests green + clean Humble build; Jazzy compile pending host-side `docker compose run` — docker is unavailable inside the Humble devcontainer)*
> - [ ] **M2 — GUI: views, icons, dropdown counts** · gate: **human manual test run** (GUI) *(implementation complete + clean Humble build; headless Xvfb/llvmpipe smoke-test renders dropdown, both views, status icons against live talker/listener. Human GUI gate + host-side Jazzy compile pending.)*

**Goal:** A lightweight C++ GUI that lists every live ROS 2 node in a dropdown; on selecting a node, shows its direct connections (publishers/subscribers it talks to) in **two interchangeable views — a table and an ego graph** — each connection carrying a single status icon (**green tick** = healthy, **amber question** = QoS incompatible, **red cross** = type mismatch or dead) that, on click, opens a popup explaining the fault in plain language (e.g. "`/map`: publisher `node_a` offers TRANSIENT_LOCAL, subscriber `node_b` requests VOLATILE → incompatible"). So a developer can diagnose a broken topic link in seconds instead of polling the CLI.

## Approach

Single ament_cmake package, `ros2_node_inspector`. **Dear ImGui core** (no node-graph library) + **OpenGL3 backend** + **GLFW** window. UI is an *ego view* of the selected node, rendered two ways behind a tab toggle: a **table** (downstream subscribers, upstream publishers) and a **graph** (selected node centered, neighbors radiating out, topics on the edges). Both render from the *same* `NodeView` model — the graph is a second renderer, not a second data path. A background `rclcpp::spin` thread keeps DDS discovery live; graph state is read each refresh tick from stable `rclcpp` graph APIs (identical on Humble and Jazzy). Liveness is measured only for the **currently selected** node's topics via short-lived `create_generic_subscription` probes, torn down on selection change.

Why these choices (verdicts from design discussion):
- **ImGui core + hand-drawn graph, not imgui-node-editor** — the ego graph is bounded (one center node + direct neighbors, tens of edges) with a fixed radial layout, and the core requirement is a *clickable status icon sitting on each edge with a popup*. imgui-node-editor models links as beziers between pins and does not expose link midpoints for custom overlays, so that requirement fights the library. Hand-drawing with `ImDrawList` gives full control of edge-midpoint icon placement and trivial click hit-testing; pan/zoom is a single offset+scale transform (~40 lines). Dependency stays dropped.
- **OpenGL3, not Vulkan/WebGPU** — the workload is trivial topology; backend is invisible to the user. OpenGL3 builds in seconds and falls back to mesa/llvmpipe software rendering inside X11-forwarded Docker. Renderer is isolated behind ImGui's `impl` layer, so a later Vulkan swap is one backend file, not a rewrite.
- **Vector-drawn status icons, not font glyphs** — tick and cross are line segments, the QoS "?" is a circle + a glyph present in the default font. Vector icons stay crisp under zoom (font glyphs blur). FontAwesome is wired in only as an optional upgrade for richer toolbar iconography (see *Icons & fonts*).
- **C++20 on both distros** — GCC 11 (Ubuntu 22.04 / Humble) supports `-std=c++20`. **Do not use `std::format`** (lands in GCC 13); use `fmt` or streams.

## Package layout

```
ros2_node_inspector/
├── package.xml
├── CMakeLists.txt
├── src/
│   ├── main.cpp               # GLFW+OpenGL3+ImGui bootstrap, frame loop
│   ├── graph_model.hpp/.cpp   # rclcpp graph reads → in-memory model
│   ├── qos_compat.hpp/.cpp    # RxO compatibility (pure, unit-testable) → QosVerdict
│   ├── liveness_probe.hpp/.cpp# generic-subscription Hz sampler
│   ├── status.hpp             # EdgeStatus enum + precedence → icon + detail string
│   ├── palette.hpp            # pinned muted color constants
│   ├── icons.hpp/.cpp         # vector-drawn tick/cross/?; optional FontAwesome merge
│   ├── view_table.hpp/.cpp    # table renderer over NodeView
│   └── view_graph.hpp/.cpp    # ego-graph renderer: radial layout, pan/zoom, edge icons
├── test/                     # gtest: qos_compat, status precedence, model parsing
├── third_party/
│   ├── imgui/                 # vendored ImGui (submodule or FetchContent)
│   └── fonts/                 # fa-solid-900.ttf (optional; agent pauses if absent)
├── .devcontainer/
│   └── devcontainer.json     # dev-only: attaches to app-humble, mounts Claude dirs
├── Dockerfile
├── docker-compose.yml
├── .gitignore
└── .vscode/{c_cpp_properties.json,tasks.json}
```

## Data model

```cpp
// Composite per-edge status (one icon per connection), in precedence order.
enum class EdgeStatus { Ok, QosMismatch, TypeMismatch, Dead, Unknown };
// → Ok = green tick · QosMismatch = amber "?" · TypeMismatch/Dead = red cross · Unknown = grey

// Returned by qos_compat so the popup can name the exact fault.
struct QosVerdict {
  bool compatible;
  std::string policy;     // e.g. "durability"  (empty if compatible)
  std::string offered;    // e.g. "TRANSIENT_LOCAL"
  std::string requested;  // e.g. "VOLATILE"
};

struct Connection {
  std::string topic;
  std::string peer_node;        // namespaced name of the node on the other end
  std::string my_type;          // type string declared by the selected node
  std::string peer_type;        // type string declared by the peer
  rclcpp::QoS  my_qos;
  rclcpp::QoS  peer_qos;
  bool         type_match;      // my_type == peer_type
  QosVerdict   qos;             // RxO compatibility (NOT operator!=)
  double       hz;              // measured rate, 0.0 if dead/unknown
  bool         hz_known;        // false until first probe window elapses
  EdgeStatus   status() const;  // precedence: type → qos → dead → ok (see status.hpp)
};

struct NodeView {
  std::string name;             // fully-qualified node name
  std::vector<Connection> publishes;   // topics this node publishes → its subscribers
  std::vector<Connection> subscribes;  // topics this node subscribes → its publishers
};
```

**`EdgeStatus::status()` precedence** (exactly one icon per edge):
1. `!type_match` → **TypeMismatch** (red cross) — "publisher sends `A`, subscriber expects `B`".
2. else `!qos.compatible` → **QosMismatch** (amber ?) — popup uses `qos.policy/offered/requested`.
3. else `hz_known && hz == 0` → **Dead** (red cross) — "compatible but no data flowing (0 Hz)".
4. else `hz_known` → **Ok** (green tick) — popup shows live rate.
5. else → **Unknown** (grey) — probe window not yet elapsed.

## rclcpp graph APIs (all stable, Humble + Jazzy)

- Node list (dropdown): `get_node_names_and_namespaces()`.
- For selected node N's own endpoints+types: `get_publisher_names_and_types_by_node(name, ns)` and `get_subscriber_names_and_types_by_node(name, ns)`.
- For each of N's topics, peer endpoints + peer QoS + peer type:
  - N publishes a topic → peers are subscribers: `get_subscriptions_info_by_topic(topic)`.
  - N subscribes a topic → peers are publishers: `get_publishers_info_by_topic(topic)`.
  - Each result is a `TopicEndpointInfo`: use `.node_name()`, `.node_namespace()`, `.topic_type()`, `.qos_profile()`.
- N's own QoS for a topic comes from the matching `TopicEndpointInfo` for N on that topic (filter the same `*_info_by_topic` list by N's name).

## QoS compatibility — Request-vs-Offered (publisher = offered, subscriber = requested)

Flag incompatible only on true RxO violation, not on any difference. Return a `QosVerdict` carrying the **first** failing policy and its offered/requested values (so the popup builds the exact sentence). Implement per-policy:

- **Reliability:** offered `BEST_EFFORT` + requested `RELIABLE` → incompatible. All other pairs OK.
- **Durability** (order VOLATILE < TRANSIENT_LOCAL < TRANSIENT < PERSISTENT): offered rank ≥ requested rank → OK; else incompatible.
- **Liveliness** (AUTOMATIC < MANUAL_BY_TOPIC): offered ≥ requested → OK; else incompatible.
- **Deadline:** offered period ≤ requested period → OK (smaller/equal is compatible; default infinite = no constraint).
- **Liveliness lease_duration:** offered ≤ requested → OK.
- **History depth:** never an incompatibility → ignore for status.

`QosVerdict{compatible=false, policy, offered, requested}` on the first violation, else `{compatible=true}`. (No partial state.)

## Liveness probe

- On selection of node N, for each connected topic create one `node->create_generic_subscription(topic, peer_type, probe_qos, cb)`.
- `probe_qos` = the **peer publisher's offered QoS profile** (adopting offered guarantees the probe is compatible; a permissive default would silently fail against a RELIABLE+TRANSIENT_LOCAL publisher and produce false "dead").
- Callback takes `std::shared_ptr<rclcpp::SerializedMessage>` — **do not deserialize**; just record arrival timestamps in a ring buffer.
- `hz` = count in trailing window / window seconds (window = 5 s). `liveness = Ok` if hz > 0, `Bad` if silent past window, `Unknown` before first window elapses.
- On selection change, destroy all probe subscriptions before creating the new set (bounded: only the selected node's topics are ever probed).

## Views

Shared chrome: a node dropdown and a **Table | Graph** tab toggle. Both views render the same `NodeView`.

**Dropdown with per-node status counts.** Each entry shows the node name followed by three small counts — `✓N  ?N  ✗N` (tick / warn / error) — aggregating that node's connections. **Counts cover type and QoS only**, which are computable from graph metadata for *every* node with no probing. **Liveness (dead/Hz) is deliberately excluded from the counts** — we never probe unselected nodes (that would mean subscribing to the whole system), so dead/Hz appears only in the detail views for the *selected* node. Mapping: `✓` = type-OK & QoS-OK, `?` = QoS-incompatible, `✗` = type-mismatch. Compute per node on the 500 ms refresh tick; cache so re-render is free.

**Table view** (`view_table`)
- Section "Publishes →" : `ImGui::BeginTable`, columns `Topic | Peer node | Type | Rate | Status`.
- Section "← Subscribes" : same for upstream publishers.
- Status cell = the vector icon for `Connection::status()`. Click the icon → popup (below). Rate shows `30 Hz` / `DEAD` / `—`.

**Graph view** (`view_graph`) — hand-drawn with `ImDrawList`
- Layout: selected node as a center box; **publishers it subscribes to placed on the left**, **subscribers it publishes to on the right**, distributed vertically (radial/columnar). Direction is read from side, so no arrowheads needed.
- Each edge = one `Connection`: a line from center to peer box, the **topic name** labelled at mid-edge, and the **status icon** drawn next to the label at a known midpoint coordinate.
- **Pan/zoom**: maintain `pan` (ImVec2) and `zoom` (float); transform every world point `screen = world*zoom + pan`. Mouse drag on empty canvas pans; wheel zooms about cursor. All hit-tests run in screen space.
- **Click-to-recenter**: clicking a peer node box sets it as the new selected node (traverse the whole system node-by-node while each view stays bounded).
- **Icon click**: hit-test cursor against each icon's screen rect; on click, open the popup anchored at the icon.

**Status popup** (shared by both views, `status.hpp` builds the text)
- ImGui popup with one or two lines:
  - QosMismatch → `"{topic}: publisher {pub} offers {offered}, subscriber {sub} requests {requested} → {policy} incompatible"`.
  - TypeMismatch → `"{topic}: {pub} publishes {my_type}, {sub} subscribes as {peer_type}"`.
  - Dead → `"{topic}: QoS+type OK but no data in last 5 s (0 Hz)"`.
  - Ok → `"{topic}: live, {hz} Hz"`.
- Click-driven (`OpenPopup` on icon click), dismiss on click-away. Same call site for table and graph.

**Refresh cadence:** graph state read every ~500 ms (discovery is slow-changing); redraw every frame at vsync.

## Icons & fonts

- **Default (no dependency):** draw status markers as `ImDrawList` vector primitives in `icons.cpp` — tick = two strokes, cross = two strokes, QoS `?` = filled amber circle + a `?` glyph from the default font. These scale crisply with graph zoom.
- **Optional FontAwesome upgrade:** if `third_party/fonts/fa-solid-900.ttf` is present, merge it into the ImGui atlas (`ImFontConfig.MergeMode`, `IconsFontAwesome6.h` `ICON_FA_*` codepoints) for richer toolbar/legend icons.
- **Pause protocol:** the build/agent attempts to fetch `fa-solid-900.ttf` (+ `IconsFontAwesome6.h`). If unavailable (e.g. offline Docker build), **pause and emit**: *"FontAwesome not found — drop `fa-solid-900.ttf` into `third_party/fonts/` and resume."* The app must still build and run without it (vector icons are the fallback, never a hard dependency).

## Palette (`palette.hpp`)

Muted dark theme — Tokyo-Night / Foxglove family, accents drawn from the desaturated end of the xkcd pool. Deliberately **not** RViz's saturated primaries. Pinned hex so "nice colors" is unambiguous; apply via `ImGui::StyleColorsDark()` then override these:

```cpp
namespace pal {  // ImU32 via IM_COL32(r,g,b,a)
  constexpr ImU32 bg       = IM_COL32(0x1A,0x1B,0x26,0xFF); // window background, deep navy-charcoal
  constexpr ImU32 panel    = IM_COL32(0x24,0x28,0x3B,0xFF); // node boxes, table rows
  constexpr ImU32 edge     = IM_COL32(0x3B,0x42,0x61,0xFF); // idle topic line / borders
  constexpr ImU32 text     = IM_COL32(0xC0,0xCA,0xF5,0xFF);
  constexpr ImU32 text_dim = IM_COL32(0x7A,0x82,0xA8,0xFF); // secondary labels, types
  constexpr ImU32 accent   = IM_COL32(0x7A,0xA2,0xF7,0xFF); // selected node, focus ring, links
  constexpr ImU32 ok       = IM_COL32(0x6B,0xC4,0x8A,0xFF); // tick   (muted sea green)
  constexpr ImU32 warn     = IM_COL32(0xE0,0xAF,0x68,0xFF); // ?      (soft amber)
  constexpr ImU32 error    = IM_COL32(0xE0,0x6C,0x75,0xFF); // cross  (soft coral, not neon red)
  constexpr ImU32 unknown  = IM_COL32(0x5C,0x63,0x70,0xFF); // grey   (probe pending)
}
```
Live edges use `accent`; dead/idle edges dim toward `edge`. Keep the window background `bg` and avoid pure white text (`text` is a soft lavender, easier on the eyes for long sessions).

## Threading

- Main thread: GLFW/ImGui frame loop.
- Background thread: `rclcpp::spin(node)` (or a `SingleThreadedExecutor`) to service discovery + probe callbacks.
- Shared model guarded by one `std::mutex`; UI copies the small model under lock each refresh tick. No lock held during ImGui rendering.

## Build / dependencies

- `package.xml`: `<depend>rclcpp</depend>`, plus build deps for GLFW/OpenGL.
- apt: `libglfw3-dev`, `libgl1-mesa-dev`, `python3-colcon-common-extensions`, `build-essential`, `cmake`, `git`. **No `libwebgpu-dev`** (does not exist).
- ImGui: vendor `third_party/imgui` (git submodule pinned to a release tag, or CMake `FetchContent`). Compile into the target: `imgui*.cpp` + `backends/imgui_impl_glfw.cpp` + `backends/imgui_impl_opengl3.cpp`.
- CMake: `find_package(ament_cmake REQUIRED)`, `find_package(rclcpp REQUIRED)`, `find_package(glfw3 REQUIRED)`, `find_package(OpenGL REQUIRED)`; `set(CMAKE_CXX_STANDARD 20)`; `set(CMAKE_EXPORT_COMPILE_COMMANDS ON)`; `ament_target_dependencies(<target> rclcpp)`; link `glfw OpenGL::GL`.

## Docker (Humble + Jazzy via one parametrized image)

`Dockerfile` — runs as non-root **`ros`** (UID/GID **1000/1000**). Note: Ubuntu 24.04 (Jazzy base) ships a default `ubuntu` user already at UID 1000, so the build must reclaim 1000 first; Humble's 22.04 does not. The conditional below handles both.
```dockerfile
ARG ROS_DISTRO=humble
FROM osrf/ros:${ROS_DISTRO}-desktop

ARG USERNAME=ros
ARG USER_UID=1000
ARG USER_GID=1000

RUN apt-get update && apt-get install -y \
    build-essential cmake git sudo \
    python3-colcon-common-extensions \
    libglfw3-dev libgl1-mesa-dev \
  && rm -rf /var/lib/apt/lists/*

# Reclaim UID 1000 if a default user (e.g. 'ubuntu' on 24.04) already holds it.
RUN if getent passwd ${USER_UID} >/dev/null; then \
      userdel -r "$(getent passwd ${USER_UID} | cut -d: -f1)" 2>/dev/null || true; \
    fi; \
    groupadd --gid ${USER_GID} ${USERNAME} 2>/dev/null || true; \
    useradd  --uid ${USER_UID} --gid ${USER_GID} -m -s /bin/bash ${USERNAME}; \
    echo "${USERNAME} ALL=(ALL) NOPASSWD:ALL" > /etc/sudoers.d/${USERNAME}; \
    chmod 0440 /etc/sudoers.d/${USERNAME}

ENV CMAKE_EXPORT_COMPILE_COMMANDS=1
USER ros
WORKDIR /home/ros/ws
```
`docker-compose.yml` (no `version:` key — obsolete in Compose v2):
```yaml
services:
  app-humble:
    build: { context: ., args: [ "ROS_DISTRO=humble" ] }
    user: "1000:1000"
    network_mode: host                                   # DDS discovery
    environment: [ "DISPLAY=${DISPLAY}" ]
    volumes:
      - .:/home/ros/ws/src/ros2_node_inspector
      - /tmp/.X11-unix:/tmp/.X11-unix                    # X11 (Linux host assumed)
  app-jazzy:
    build: { context: ., args: [ "ROS_DISTRO=jazzy" ] }
    user: "1000:1000"
    network_mode: host
    environment: [ "DISPLAY=${DISPLAY}" ]
    volumes:
      - .:/home/ros/ws/src/ros2_node_inspector
      - /tmp/.X11-unix:/tmp/.X11-unix
```
Host: `xhost +local:` before launch (UID 1000 matching the host user keeps the X11 cookie valid). Build/run inside the container from `/home/ros/ws`. macOS/Windows hosts need XQuartz/VcXsrv (note in README, out of scope).

## VS Code

`.vscode/c_cpp_properties.json` — the repo *is* the package, mounted at `/home/ros/ws/src/ros2_node_inspector`, but colcon builds at the workspace root `/home/ros/ws`, so the compile DB lives **outside** the repo at `/home/ros/ws/build/compile_commands.json`. Use that absolute container path (not `${workspaceFolder}/build`). The C/C++ extension runs **inside** the devcontainer, so it reads this path directly:
```json
{ "version": 4, "configurations": [{
  "name": "ROS 2 Linux",
  "compilerPath": "/usr/bin/gcc",
  "intelliSenseMode": "linux-gcc-x64",
  "compileCommands": "/home/ros/ws/build/compile_commands.json",
  "cStandard": "c17", "cppStandard": "c++20",
  "configurationProvider": "ms-vscode.cmake-tools"
}]}
```
`.vscode/tasks.json` (run from the workspace root `/home/ros/ws`):
```json
{ "version": "2.0.0", "tasks": [{
  "label": "colcon build", "type": "shell",
  "command": "colcon build --symlink-install --merge-install --cmake-args -DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
  "options": { "cwd": "/home/ros/ws" },
  "group": { "kind": "build", "isDefault": true },
  "problemMatcher": "$gcc"
}]}
```
(`--merge-install` yields the single `/home/ros/ws/build/compile_commands.json`.)

## Devcontainer (dev-only, single target = Humble)

**Decision:** use a compose-based devcontainer attached to **`app-humble`**. The C/C++ extension runs inside the container, so IntelliSense + `compile_commands.json` work fully — the `docker exec`-only alternative would lose in-editor completion for no gain (no ROS on the host to resolve `rclcpp` headers). **Jazzy is verified non-interactively** with `docker compose run --rm app-jazzy colcon build` — it's a compatibility gate, not a daily-dev surface. Develop on the older toolchain (Humble/GCC 11) so toolchain-version issues surface early.

Claude config and dev-only mounts live here, **not** in compose (compose is the runtime contract):
```jsonc
// .devcontainer/devcontainer.json
{
  "name": "ros2_node_inspector (humble)",
  "dockerComposeFile": ["../docker-compose.yml"],
  "service": "app-humble",
  "workspaceFolder": "/home/ros/ws/src/ros2_node_inspector",
  "remoteUser": "ros",
  "features": {
    "ghcr.io/devcontainers/features/node:1": {}        // for Claude Code CLI
  },
  "mounts": [
    // Claude Code config/auth/history — dev-only, host → container 'ros' home
    "source=${localEnv:HOME}/.claude,target=/home/ros/.claude,type=bind",
    "source=${localEnv:HOME}/.claude.json,target=/home/ros/.claude.json,type=bind"
  ],
  "postCreateCommand": "npm install -g @anthropic-ai/claude-code",  // verify current install cmd at docs.claude.com
  "customizations": {
    "vscode": {
      "extensions": ["ms-vscode.cpptools", "ms-vscode.cmake-tools", "ms-iot.vscode-ros"]
    }
  }
}
```
Notes: the `~/.claude.json` bind requires the file to exist on the host first (`touch ~/.claude.json` if not). Because the devcontainer reuses the compose `app-humble` service, it inherits `network_mode: host`, the X11 mount, and the source bind — so ROS discovery and the GUI work the same inside the devcontainer as at runtime. Confirm the Claude Code install command against current docs (package name may change).

## .gitignore

```gitignore
# colcon
build/
install/
log/

# compile DB symlink (if created at root)
/compile_commands.json

# editor / OS
.cache/
*.swp
.DS_Store

# vendored third-party build artifacts (keep sources, ignore builds)
third_party/**/build/

# downloaded font (large binary; user supplies per pause protocol)
third_party/fonts/*.ttf
```
(Keep `.vscode/` committed — those configs are authored deliverables. The FontAwesome TTF is git-ignored since the user pastes it in locally.)

## Milestones

Work one milestone per run (see Agent Protocol at top). Each lists its tasks and its **done-gate**.

### M0 — Project setup  ·  gate: container comes up + first commit
*No app code. Goal: a human can open the repo as a devcontainer and launch Claude inside it.*
- [x] Create `Dockerfile` (non-root `ros`, UID/GID 1000, UID-reclaim conditional), `docker-compose.yml` (both services, mount at `/home/ros/ws/src`), `.gitignore`, `.vscode/{c_cpp_properties.json,tasks.json}`, `.devcontainer/devcontainer.json`.
- [x] Build both images: `docker compose build app-humble app-jazzy`. Confirm the UID-1000 reclaim succeeds on Jazzy (24.04). *(Verified: `ros` = uid 1000 on both Humble and Jazzy.)*
- [x] `git init`, stage all, `git commit -m "first commit, project setup"`.
- [x] **Gate (human):** open the folder in VS Code → "Reopen in Container" (attaches `app-humble`); confirm it builds, `ros` is the user (`whoami`), `~/.claude` is mounted, and `claude` launches. No compile/test step — there's no code yet.

### M1 — Backend & diagnostics  ·  gate: unit tests pass (non-GUI)
*Pure logic + ROS graph reads. No window. Everything here is gtest-verifiable.*
- [x] Scaffold ament_cmake package: `package.xml` (`<depend>rclcpp</depend>`), `CMakeLists.txt` (`CMAKE_CXX_STANDARD 20`, export compile commands), `test/` wired with `ament_add_gtest`.
- [x] `graph_model`: build `NodeView` from the rclcpp graph APIs; resolve peers/types/QoS for a selected node. *(by-node listers live on `NodeGraphInterface`, not `Node`.)*
- [x] `qos_compat` → `QosVerdict`: RxO per-policy with failing policy + offered/requested.
- [x] `status.hpp`: `EdgeStatus::status()` precedence + popup-text builder.
- [x] `liveness_probe`: generic subscriptions adopting peer offered QoS, trailing-window Hz, create/destroy on selection change. *(probe QoS must normalise history: TopicEndpointInfo reports history=UNKNOWN/depth=0 over DDS discovery, which rcl rejects with a misleading "invalid allocator"; history isn't part of RxO so we force KEEP_LAST while preserving the discoverable policies.)*
- [x] Dropdown count aggregation (type/QoS only, all nodes) as a pure function over the model.
- [x] **Tests:** qos_compat (BEST_EFFORT pub→RELIABLE sub = incompatible/reliability; RELIABLE→BEST_EFFORT = OK; VOLATILE pub→TRANSIENT_LOCAL sub = incompatible/durability); status precedence (type beats qos beats dead); count aggregation; RateWindow Hz math. graph_model + liveness verified via the `inspect_cli` manual harness against `talker`/`listener` (`/chatter` reads live 4 Hz, `/parameter_events` reads DEAD).
- [x] **Gate:** `colcon test` green on Humble (28 tests). ⚠ `docker compose run --rm app-jazzy colcon build` not run — docker is unavailable inside the Humble devcontainer; **run on the host to confirm Jazzy**.

### M2 — GUI: views, icons, dropdown counts  ·  gate: human manual test run (GUI)
*Everything visual. Renders the M1 model.*
- [x] Vendor ImGui under `third_party/imgui` (v1.91.5, sources committed; examples/docs trimmed); wire core + `imgui_impl_glfw` + `imgui_impl_opengl3` into the target as a static `imgui` lib.
- [x] `main.cpp`: GLFW + OpenGL3 + ImGui bootstrap. Threading refined vs. the original sketch: a single background **ROS thread** owns the node/executor/probe — it `spin_some`es and, every 500 ms, builds node list + counts + selected `NodeView` and publishes a snapshot under one mutex; the UI thread copies that snapshot under lock and renders (no rclcpp on the UI thread, no lock during ImGui). This keeps all subscription lifetimes on one thread, avoiding the create/destroy races the "background spin + shared model" note warned about. Per-frame redraw at vsync. `palette.hpp` applied.
- [x] `icons`: vector tick/cross/? drawn with `ImDrawList`. `fa-solid-900.ttf` **was fetched** (FortAwesome 6.x) and is merged when present — no pause needed. Base UI font is system DejaVuSans (for `→ ← — ●` glyphs) with the ImGui bitmap default as fallback.
- [x] Dropdown with `✓N ?N ✗N` per-node counts (FA glyphs when loaded, else `ok/?/x` letters), computed by the ROS thread for every node from `count_connections`.
- [x] `view_table`: "Publishes →" / "← Subscribes" tables + clickable vector status icons + shared click popup.
- [x] `view_graph`: radial ego layout (`ImDrawList`) — subscribers right, publishers left, selected node centered; pan (drag) / zoom-about-cursor (wheel); mid-edge topic labels (declutter below zoom 0.55) + status icons; screen-space hit-testing for icon-click popup and click-to-recenter; Table|Graph tab toggle.
- [x] README: launch, `xhost`, host-OS X11 caveat, distro matrix, FontAwesome note.
- [ ] **Gate (human):** run on a live `talker`/`listener` (+ deliberately mismatched QoS pair); confirm dropdown counts, both views, status icons, popups with correct sentences, click-to-recenter, pan/zoom. Then `docker compose run --rm app-jazzy colcon build` compiles. (Optional: run under TSan once to check the spin/UI race.)
  - *Agent-side progress: clean Humble build + all 28 M1 tests still green. Headless Xvfb + llvmpipe smoke-test ran the full frame loop ~8 s against live talker/listener with no crash; screenshots confirm the dropdown, both table sections, the radial graph with edges/labels/icons, and the muted palette render correctly. **Still requires a human** to verify interactive behaviour (icon-click popups, click-to-recenter, pan/zoom) on a real display, plus the host-side Jazzy compile (docker is unavailable inside the Humble devcontainer).*

## Edge cases & risks

- **False "dead" from probe QoS:** mitigated by adopting the publisher's offered QoS for the probe (see Liveness). Still impossible to receive if publisher offers something exotic — surface **Unknown** (grey), not Dead, when no compatible probe can be created.
- **Type comparison:** compare full type strings (`pkg/msg/Type`); a mismatch usually co-occurs with no connection at all — surface as red cross.
- **Graph density:** a node with many peers (e.g. 30 subscribers) crowds the radial layout. MVP relies on pan/zoom + vertical distribution; if it bites, add per-side scrolling or namespace grouping (deferred). Edge labels should de-clutter on zoom-out (draw icon only, hide topic text below a zoom threshold).
- **Click hit-testing under zoom:** run all icon/node hit-tests in screen space (post-transform), not world space, or clicks drift as you zoom.
- **Graph churn:** nodes appearing/disappearing mid-view → guard against the selected node vanishing (fall back to empty view, keep dropdown responsive); click-to-recenter must validate the target still exists.
- **Probe cost:** bounded to the selected node's topics; for high-rate topics (images) count timestamps only, never copy/deserialize payloads.
- **FontAwesome absent:** vector icons are the fallback, so the build never hard-fails on a missing TTF — the pause is a prompt, not a blocker.
- **Docker UID 1000 collision:** 24.04 (Jazzy) base ships `ubuntu` at UID 1000; the reclaim conditional must run or `useradd` fails. Verify Jazzy build explicitly, not just Humble.
- **Devcontainer single-target:** the editor attaches to Humble only; Jazzy is build-verified via `docker compose run`, not interactively. Acceptable — Jazzy is a compatibility gate. Don't let Humble-only iteration hide a Jazzy break; M1/M2 gates both include the Jazzy compile.
- **Dropdown count cost:** computing type/QoS counts for *every* node each refresh is O(total endpoints). Fine for tens of nodes; for very large systems, compute lazily or throttle to every Nth tick (deferred).
- **`~/.claude.json` bind:** the file must exist on the host before the container starts, else Docker creates a directory in its place. README must instruct `touch ~/.claude.json`.
- **Software rendering:** llvmpipe is fine for this draw load; no GPU passthrough required.

## Open questions

None blocking. Deferred (post-MVP): namespace grouping/collapsing in the graph for dense nodes; dropdown namespace filtering; persisting last-selected node and graph pan/zoom; optional Vulkan backend swap; richer FontAwesome legend if the TTF is supplied.
