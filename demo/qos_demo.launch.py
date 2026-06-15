"""Spin up a small fake graph for testing the inspector.

Three talker/listener pairs, distinctly named, so the inspector shows one of
each status:

  talker_1 / listener_1  on /demo_ok           RELIABLE/VOLATILE  both  -> healthy (green tick, live Hz)
  talker_2 / listener_2  on /demo_reliability  BEST_EFFORT pub vs RELIABLE sub  -> QoS "?" (reliability)
  talker_3 / listener_3  on /demo_durability   VOLATILE pub vs TRANSIENT_LOCAL sub -> QoS "?" (durability)

Run with:
    ros2 launch connection_inspector qos_demo.launch.py      # if installed
    ros2 launch ./demo/qos_demo.launch.py                   # straight from source

Each node is a plain python3 invocation of qos_demo_node.py (no build/install
step required). Ctrl-C tears the whole graph down.
"""

import os
import sys

from launch import LaunchDescription
from launch.actions import ExecuteProcess

THIS_DIR = os.path.dirname(os.path.abspath(__file__))
NODE = os.path.join(THIS_DIR, "qos_demo_node.py")


def node(name, role, topic, reliability, durability, rate=2.0):
    return ExecuteProcess(
        cmd=[
            sys.executable, NODE, "--ros-args",
            "-r", f"__node:={name}",
            "-p", f"role:={role}",
            "-p", f"topic:={topic}",
            "-p", f"reliability:={reliability}",
            "-p", f"durability:={durability}",
            "-p", f"rate:={rate}",
        ],
        name=name,
        output="screen",
    )


def generate_launch_description():
    return LaunchDescription([
        # Healthy: matching RELIABLE / VOLATILE -> data flows, green tick.
        node("talker_1",   "talker",   "/demo_ok",          "reliable",    "volatile"),
        node("listener_1", "listener", "/demo_ok",          "reliable",    "volatile"),

        # Reliability mismatch: BEST_EFFORT publisher vs RELIABLE subscriber.
        node("talker_2",   "talker",   "/demo_reliability", "best_effort", "volatile"),
        node("listener_2", "listener", "/demo_reliability", "reliable",    "volatile"),

        # Durability mismatch: VOLATILE publisher vs TRANSIENT_LOCAL subscriber.
        node("talker_3",   "talker",   "/demo_durability",  "reliable",    "volatile"),
        node("listener_3", "listener", "/demo_durability",  "reliable",    "transient_local"),
    ])
