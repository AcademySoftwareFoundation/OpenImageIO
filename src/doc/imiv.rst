..
  Copyright Contributors to the OpenImageIO project.
  SPDX-License-Identifier: CC-BY-4.0


.. _chap-imiv:

`imiv`: ImGui Image Viewer
################################

.. highlight:: bash


Overview
========

The :program:`imiv` program is an in-progress Dear ImGui-based replacement
for the legacy :program:`iv` image viewer. It uses GLFW for windowing and
currently supports renderer backends built around Vulkan, Metal, or OpenGL.

ImIv port is still under active development, and several of the most
valuable workflows are developer-facing:

* understanding the shared app/viewer/UI architecture;
* keeping behavior aligned across multiple renderer backends;
* extending the automated GUI regression suite built on Dear ImGui Test
  Engine.


Current status
==============

:program:`imiv` already covers the core image-viewing loop, OCIO display
controls, backend selection, persistent preferences, and a growing automated
regression suite. But it does **not** yet claim full parity with
:program:`iv`.

In particular:

* Vulkan is currently the reference backend for renderer-side feature work.
* Current macOS multi-backend verification is green on Vulkan, Metal, and
  OpenGL in the shared backend suite.
* Multi-backend builds can expose backend selection through both the command
  line and the Preferences window, with restart-on-next-launch semantics.
* Some legacy :program:`iv` workflows are still intentionally marked as
  incomplete or behaviorally different.

For that reason, this chapter is organized into a compact user guide and
deeper developer/testing guides.


Documentation map
=================

.. toctree::
   :maxdepth: 1

   imiv_user
   imiv_dev
   imiv_tests
