#!/usr/bin/env python
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Runs a python script under an isolate

This script attempts to emulate the contract of gtest-style tests
invoked via recipes.

If optional argument --isolated-script-test-output=[FILENAME] is passed
to the script, json is written to that file in the format detailed in
//docs/testing/json-test-results-format.md.

This script is intended to be the base command invoked by the isolate,
followed by a subsequent Python script."""

import argparse
import json
import os
import sys

# Add src/testing/ into sys.path for importing xvfb and common.
sys.path.append(
    os.path.abspath(os.path.join(os.path.dirname(__file__), os.path.pardir)))
import xvfb
from scripts import common

# pylint: disable=super-with-arguments


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--isolated-script-test-output', type=str)
  args, _ = parser.parse_known_args()

  if sys.platform == 'win32':
    exe = os.path.join('.', 'flatbuffers_unittests.exe')
  else:
    exe = os.path.join('.', 'flatbuffers_unittests')

  env = os.environ.copy()
  failures = []
  with common.temporary_file() as tempfile_path:
    rc = xvfb.run_executable([exe], env, stdoutfile=tempfile_path)

    # The flatbuffer tests do not really conform to anything parsable, except
    # that they will succeed with "ALL TESTS PASSED". We cannot test for
    # equality because some tests operate on invalid input and produce error
    # messages (e.g. "Field id in struct ProtoMessage has a non positive number
    # value" in a test that verifies behavior if a proto message does contain
    # a non positive number).
    with open(tempfile_path) as f:
      output = f.read()
      if "ALL TESTS PASSED\n" not in output:
        failures = [output]

  if args.isolated_script_test_output:
    with open(args.isolated_script_test_output, 'w') as fp:
      json.dump({'valid': True, 'failures': failures}, fp)

  return rc


def main_compile_targets(args):
  json.dump(['flatbuffers_unittests'], args.output)


if __name__ == '__main__':
  # Conform minimally to the protocol defined by ScriptTest.
  if 'compile_targets' in sys.argv:
    funcs = {
        'run': None,
        'compile_targets': main_compile_targets,
    }
    sys.exit(common.run_script(sys.argv[1:], funcs))
  sys.exit(main())
