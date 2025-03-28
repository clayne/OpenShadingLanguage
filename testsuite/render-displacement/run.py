#!/usr/bin/env python

# Copyright Contributors to the Open Shading Language project.
# SPDX-License-Identifier: BSD-3-Clause
# https://github.com/AcademySoftwareFoundation/OpenShadingLanguage

failthresh = 0.01
failpercent = 1
outputs = [ "out.exr" ]
command = testrender("-r 256 256 -aa 8 scene.xml out.exr")
