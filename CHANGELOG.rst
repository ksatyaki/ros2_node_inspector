^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Changelog for package connection_inspector
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

1.0.0 (2026-06-15)
------------------

* Refinements
* docs: add GPLv3 license and update package.xml
* feat: complete UI tasks and add GitHub Actions for release
* M1: backend & diagnostics (graph model, QoS RxO, status, liveness)
  Scaffold the ament_cmake package and the non-GUI core:
  - qos_compat: Request-vs-Offered compatibility -> QosVerdict (first failing
  policy + offered/requested values)
  - graph_model: build NodeView from the live rclcpp graph (by-node listers
  via NodeGraphInterface); pure type/QoS count aggregation for the dropdown
  - status: EdgeStatus precedence (type>qos>dead>ok>unknown) + popup text
  - liveness_probe: generic-subscription Hz sampler over a trailing window;
  normalises probe QoS history (TopicEndpointInfo reports UNKNOWN/depth 0,
  which rcl rejects as "invalid allocator"); history is not part of RxO
  - inspect_cli: manual harness verifying graph_model + liveness against
  talker/listener
  Gate: 28 gtests green on Humble; clean build. Jazzy compile still to be
  run on the host (docker unavailable inside the Humble devcontainer).
  Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>

