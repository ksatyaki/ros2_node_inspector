#!/usr/bin/env python3
"""Parametrized talker/listener used by qos_demo.launch.py.

One process = one node. Role, topic and QoS (reliability/durability) come from
ROS parameters so the launch file can spin up pairs that are QoS-compatible
(green tick in the inspector) or deliberately incompatible (amber "?"). The node
name is set by the launch file via `-r __node:=...`.
"""

import rclpy
from rclpy.node import Node
from rclpy.qos import (
    QoSProfile,
    QoSReliabilityPolicy,
    QoSDurabilityPolicy,
    QoSHistoryPolicy,
)
from std_msgs.msg import String


def make_qos(reliability: str, durability: str, depth: int = 10) -> QoSProfile:
    qos = QoSProfile(depth=depth)
    qos.history = QoSHistoryPolicy.KEEP_LAST
    qos.reliability = (
        QoSReliabilityPolicy.RELIABLE
        if reliability == "reliable"
        else QoSReliabilityPolicy.BEST_EFFORT
    )
    qos.durability = (
        QoSDurabilityPolicy.TRANSIENT_LOCAL
        if durability == "transient_local"
        else QoSDurabilityPolicy.VOLATILE
    )
    return qos


class QosDemoNode(Node):
    def __init__(self):
        super().__init__("qos_demo_node")
        self.declare_parameter("role", "talker")          # talker | listener
        self.declare_parameter("topic", "/demo")
        self.declare_parameter("reliability", "reliable")  # reliable | best_effort
        self.declare_parameter("durability", "volatile")   # volatile | transient_local
        self.declare_parameter("rate", 2.0)

        role = self.get_parameter("role").value
        topic = self.get_parameter("topic").value
        rel = self.get_parameter("reliability").value
        dur = self.get_parameter("durability").value
        rate = float(self.get_parameter("rate").value)
        qos = make_qos(rel, dur)

        if role == "talker":
            self._pub = self.create_publisher(String, topic, qos)
            self._i = 0
            self.create_timer(1.0 / max(rate, 0.1), self._tick)
            self.get_logger().info(f"talker  {topic}  [{rel}/{dur}]  @ {rate} Hz")
        else:
            self._sub = self.create_subscription(String, topic, self._cb, qos)
            self.get_logger().info(f"listener {topic}  [{rel}/{dur}]")

    def _tick(self):
        msg = String()
        msg.data = f"hello {self._i}"
        self._i += 1
        self._pub.publish(msg)

    def _cb(self, _msg):
        pass


def main():
    rclpy.init()
    node = QosDemoNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.try_shutdown()


if __name__ == "__main__":
    main()
