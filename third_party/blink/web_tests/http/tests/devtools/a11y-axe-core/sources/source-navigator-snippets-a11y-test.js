// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function () {
  TestRunner.addResult('Tests accessibility in the Sources panel Navigator pane Snippets tab using axe-core.');

  // axe-core issue #1444 -- role="tree" requires children with role="treeitem",
  // but it is reasonable to have trees with no leaves.
  const NO_REQUIRED_CHILDREN_RULESET = {
    'aria-required-children': {
      enabled: false,
    },
  };

  await TestRunner.loadModule('axe_core_test_runner');
  await TestRunner.loadModule('sources_test_runner');

  await UI.viewManager.showView('sources');

  await setup();
  await testA11yForView(NO_REQUIRED_CHILDREN_RULESET);

  TestRunner.completeTest();

  async function setup() {
    const snippetsProject = Snippets.ScriptSnippetFileSystem.findSnippetsProject();
    // Add snippets
    await snippetsProject.createFile('s1', null, '');
    await snippetsProject.createFile('s2', null, '');
  }

  async function testA11yForView(ruleSet) {
    await UI.viewManager.showView('navigator-snippets');
    const sourcesNavigatorView = new Sources.SnippetsNavigatorView();

    sourcesNavigatorView.show(UI.inspectorView.element);
    SourcesTestRunner.dumpNavigatorView(sourcesNavigatorView);
    const element = UI.panels.sources._navigatorTabbedLocation._tabbedPane.element;
    await AxeCoreTestRunner.runValidation(element, ruleSet);
  }
})();
