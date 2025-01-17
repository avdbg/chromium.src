# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Methods related to querying builder information from Buildbucket."""

import fnmatch
import json
import logging
import os
import subprocess

from unexpected_passes import multiprocessing_utils

TESTING_BUILDBOT_DIR = os.path.realpath(
    os.path.join(os.path.dirname(__file__), '..', '..', '..', '..', 'testing',
                 'buildbot'))

AUTOGENERATED_JSON_KEY = 'AAAAA1 AUTOGENERATED FILE DO NOT EDIT'

GPU_TELEMETRY_ISOLATES = {
    'fuchsia_telemetry_gpu_integration_test',
    'telemetry_gpu_integration_test',
}

# There are a few CI builders that don't actually exist, but have trybot
# mirrors. So, define a manual mapping here.
# Go from try -> CI then reverse the mapping so that there's less of a chance of
# typos being introduced in the repeated trybot names.
FAKE_TRY_BUILDERS = {
    # chromium.gpu.fyi
    'android_angle_rel_ng': [
        'ANGLE GPU Android Release (Nexus 5X)',
    ],
    'android_optional_gpu_tests_rel': [
        'Optional Android Release (Nexus 5X)',
    ],
    'linux-angle-rel': [
        'ANGLE GPU Linux Release (Intel HD 630)',
        'ANGLE GPU Linux Release (NVIDIA)',
    ],
    'linux_optional_gpu_tests_rel': [
        'Optional Linux Release (Intel HD 630)',
        'Optional Linux Release (NVIDIA)',
    ],
    'mac-angle-rel': [
        'ANGLE GPU Mac Release (Intel)',
        'ANGLE GPU Mac Retina Release (AMD)',
        'ANGLE GPU Mac Retina Release (NVIDIA)',
    ],
    'mac_optional_gpu_tests_rel': [
        'Optional Mac Release (Intel)',
        'Optional Mac Retina Release (AMD)',
        'Optional Mac Retina Release (NVIDIA)',
    ],
    'win-angle-rel-32': [
        'Win7 ANGLE Tryserver (AMD)',
    ],
    'win-angle-rel-64': [
        'ANGLE GPU Win10 x64 Release (Intel HD 630)',
        'ANGLE GPU Win10 x64 Release (NVIDIA)',
    ],
    'win_optional_gpu_tests_rel': [
        'Optional Win10 x64 Release (Intel HD 630)',
        'Optional Win10 x64 Release (NVIDIA)',
    ],
}
FAKE_CI_BUILDERS = {}
for try_builder, ci_builder_list in FAKE_TRY_BUILDERS.iteritems():
  for ci in ci_builder_list:
    FAKE_CI_BUILDERS[ci] = try_builder

# There are some builders that aren't under the Chromium Buildbucket project
# but are listed in the Chromium //testing/buildbot files. These don't use the
# same recipes as Chromium builders, and thus don't have the list of trybot
# mirrors.
NON_CHROMIUM_BUILDERS = {
    'Win V8 FYI Release (NVIDIA)',
    'Mac V8 FYI Release (Intel)',
    'Linux V8 FYI Release - pointer compression (NVIDIA)',
    'Linux V8 FYI Release (NVIDIA)',
    'Android V8 FYI Release (Nexus 5X)',
}


def GetCiBuilders(suite):
  """Gets the set of CI builders to query.

  Args:
    suite: A string containing the suite (as known by Telemetry) that will be
        queried. Used to filter out builders that don't actually run the suite
        in question.

  Returns:
    A set of strings, each element being the name of a Chromium CI builder to
    query results from.
  """
  logging.info('Getting CI builders')
  ci_builders = set()
  for buildbot_file in os.listdir(TESTING_BUILDBOT_DIR):
    if not buildbot_file.endswith('.json'):
      continue
    filepath = os.path.join(TESTING_BUILDBOT_DIR, buildbot_file)
    with open(filepath) as f:
      buildbot_json = json.load(f)
    # Skip any JSON files that don't contain builder information.
    if AUTOGENERATED_JSON_KEY not in buildbot_json:
      continue

    for builder, test_map in buildbot_json.iteritems():
      # Remove compile-only builders and the auto-generated comments.
      if 'Builder' in builder or 'AAAA' in builder:
        continue
      # Filter out any builders that don't run the suite in question.
      if not _SuiteInTests(suite, test_map.get('isolated_scripts', [])):
        continue
      ci_builders.add(builder)
  logging.debug('Got %d CI builders after trimming: %s', len(ci_builders),
                ci_builders)
  return ci_builders


def _SuiteInTests(suite, tests):
  """Determines if |suite| is run as part of |tests|.

  Args:
    suite: A string containing the suite (as known by Telemetry).
    tests: A list of dictionaries, each dictionary containing a test definition
        as found in the //testing/buildbot JSON files.

  Returns:
    True if |suite| is run as part of |tests|, else False.
  """
  for t in tests:
    if t.get('isolate_name') not in GPU_TELEMETRY_ISOLATES:
      continue
    if suite in t.get('args', []):
      return True
  return False


def GetTryBuilders(ci_builders):
  """Gets the set of try builders to query.

  A try builder is of interest if it mirrors a builder in |ci_builders|.

  Args:
    ci_builders: An iterable of strings, each element being the name of a
        Chromium CI builder that results will be/were queried from.

  Returns:
    A set of strings, each element being the name of a Chromium try builder to
    query results from.
  """
  logging.info('Getting try builders')
  mirrored_builders = set()
  no_output_builders = set()

  pool = multiprocessing_utils.GetProcessPool()
  results = pool.map(_GetMirroredBuildersForCiBuilder, ci_builders)
  for (builders, found_mirror) in results:
    if found_mirror:
      mirrored_builders |= builders
    else:
      no_output_builders |= builders

  if no_output_builders:
    raise RuntimeError(
        'Did not get Buildbucket output for the following builders. They may '
        'need to be added to the FAKE_TRY_BUILDERS or NON_CHROMIUM_BUILDERS '
        'mappings.\n%s' % '\n'.join(no_output_builders))
  logging.debug('Got %d try builders: %s', len(mirrored_builders),
                mirrored_builders)
  return mirrored_builders


def _GetMirroredBuildersForCiBuilder(ci_builder):
  """Gets the set of try builders that mirror a CI builder.

  Args:
    ci_builder: A string containing the name of a Chromium CI builder.

  Returns:
    A tuple (builders, found_mirror). |builders| is a set of strings, either the
    set of try builders that mirror |ci_builder| or |ci_builder|, depending on
    the value of |found_mirror|. |found_mirror| is True if mirrors were actually
    found, in which case |builders| contains the try builders. Otherwise,
    |found_mirror| is False and |builders| contains |ci_builder|.
  """
  mirrored_builders = set()
  if ci_builder in NON_CHROMIUM_BUILDERS:
    logging.debug('%s is a non-Chromium CI builder', ci_builder)
    return mirrored_builders, True

  if ci_builder in FAKE_CI_BUILDERS:
    mirrored_builders.add(FAKE_CI_BUILDERS[ci_builder])
    logging.debug('%s is a fake CI builder mirrored by %s', ci_builder,
                  FAKE_CI_BUILDERS[ci_builder])
    return mirrored_builders, True

  bb_output = _GetBuildbucketOutputForCiBuilder(ci_builder)
  if not bb_output:
    mirrored_builders.add(ci_builder)
    logging.debug('Did not get Buildbucket output for builder %s', ci_builder)
    return mirrored_builders, False

  bb_json = json.loads(bb_output)
  mirrored = bb_json.get('output', {}).get('properties',
                                           {}).get('mirrored_builders', [])
  # The mirror names from Buildbucket include the group separated by :, e.g.
  # tryserver.chromium.android:gpu-fyi-try-android-m-nexus-5x-64, so only grab
  # the builder name.
  for mirror in mirrored:
    split = mirror.split(':')
    assert len(split) == 2
    logging.debug('Got mirrored builder for %s: %s', ci_builder, split[1])
    mirrored_builders.add(split[1])
  return mirrored_builders, True


def _GetBuildbucketOutputForCiBuilder(ci_builder):
  # Ensure the user is logged in to bb.
  if not _GetBuildbucketOutputForCiBuilder.authenticated:
    try:
      with open(os.devnull, 'w') as devnull:
        subprocess.check_call(['bb', 'auth-info'],
                              stdout=devnull,
                              stderr=devnull)
    except:
      raise RuntimeError('You are not logged into bb - run `bb auth-login`.')
    _GetBuildbucketOutputForCiBuilder.authenticated = True
  # Split out for ease of testing.
  # Get the Buildbucket ID for the most recent completed build for a builder.
  p = subprocess.Popen([
      'bb',
      'ls',
      '-id',
      '-1',
      '-status',
      'ended',
      'chromium/ci/%s' % ci_builder,
  ],
                       stdout=subprocess.PIPE)
  # Use the ID to get the most recent build.
  bb_output = subprocess.check_output([
      'bb',
      'get',
      '-A',
      '-json',
  ],
                                      stdin=p.stdout)
  return bb_output


_GetBuildbucketOutputForCiBuilder.authenticated = False
