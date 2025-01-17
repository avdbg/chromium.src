// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Defines a custom Polymer component for the ChromeVox
 * interactive tutorial engine.
 */

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Curriculum, InteractionMedium, LessonData, MainMenuButtonData, Screen} from './constants.js';
import {LessonContainer} from './lesson_container.js';
import {LessonMenu} from './lesson_menu.js';
import {Localization} from './localization.js';
import {MainMenu} from './main_menu.js';
import {NavigationButtons} from './navigation_buttons.js';
import {TutorialBehavior} from './tutorial_behavior.js';
import {TutorialLesson} from './tutorial_lesson.js';

/**
 * The types of nudges given by the tutorial.
 * General nudges: announce the current item three times, then give two general
 * hints about how to navigate with ChromeVox, then a final nudge about how to
 * exit the tutorial.
 * Practice area nudges: specified by the |hints| array in lessonData. These
 * are nudges for the practice area and are only given when the practice area
 * is active.
 * @enum {string}
 */
const NudgeType = {
  GENERAL: 'general',
  PRACTICE_AREA: 'practice_area'
};

Polymer({
  is: 'chromevox-tutorial',

  _template: html`{__html_template__}`,

  behaviors: [Localization, TutorialBehavior],

  properties: {
    /** @private {boolean} */
    interactiveMode_: {type: Boolean, value: false},

    /** @private {number} */
    nudgeIntervalId_: {type: Number},

    /**
     * @const {number}
     * @private
     */
    NUDGE_INTERVAL_TIME_MS_: {type: Number, value: 45 * 1000},

    /** @private {number} */
    nudgeCounter_: {type: Number, value: 0},

    /** @private {Array<function(): void>} */
    nudgeArray_: {type: Array},

    /** @type {Array<!MainMenuButtonData>} */
    mainMenuButtonData: {
      type: Array,
      value: [
        {
          title: 'tutorial_quick_orientation_title',
          curriculum: Curriculum.QUICK_ORIENTATION,
        },
        {
          title: 'tutorial_essential_keys_title',
          curriculum: Curriculum.ESSENTIAL_KEYS,
        },
        {
          title: 'tutorial_navigation_title',
          curriculum: Curriculum.NAVIGATION,
        },
        {
          title: 'tutorial_command_references_title',
          curriculum: Curriculum.COMMAND_REFERENCES,
        },
        {
          title: 'tutorial_sounds_and_settings_title',
          curriculum: Curriculum.SOUNDS_AND_SETTINGS,
        },
        {
          title: 'tutorial_resources_title',
          curriculum: Curriculum.RESOURCES,
        }
      ]
    },

    /** @type {Array<!LessonData>} */
    lessonData: {
      type: Array,
      value: [
        {
          title: 'tutorial_welcome_heading',
          content: ['tutorial_quick_orientation_intro_text'],
          medium: InteractionMedium.KEYBOARD,
          curriculums: [Curriculum.QUICK_ORIENTATION],
          actions: [
            {type: 'key_sequence', value: {keys: {keyCode: [32 /* Space */]}}}
          ],
          autoInteractive: true,
        },

        {
          title: 'tutorial_quick_orientation_control_title',
          content: ['tutorial_quick_orientation_control_text'],
          medium: InteractionMedium.KEYBOARD,
          curriculums: [Curriculum.QUICK_ORIENTATION],
          actions: [{
            type: 'key_sequence',
            value: {keys: {keyCode: [17 /* Ctrl */]}},
            shouldPropagate: false
          }],
          autoInteractive: true,
        },

        {
          title: 'tutorial_quick_orientation_shift_title',
          content: ['tutorial_quick_orientation_shift_text'],
          medium: InteractionMedium.KEYBOARD,
          curriculums: [Curriculum.QUICK_ORIENTATION],
          actions: [
            {type: 'key_sequence', value: {keys: {keyCode: [16 /* Shift */]}}}
          ],
          autoInteractive: true,
        },

        {
          title: 'tutorial_quick_orientation_search_title',
          content: ['tutorial_quick_orientation_search_text'],
          medium: InteractionMedium.KEYBOARD,
          curriculums: [Curriculum.QUICK_ORIENTATION],
          actions: [{
            type: 'key_sequence',
            value: {skipStripping: false, keys: {keyCode: [91 /* Search */]}},
          }],
          autoInteractive: true,
        },

        {
          title: 'tutorial_quick_orientation_basic_navigation_title',
          content: [
            'tutorial_quick_orientation_basic_navigation_move_text',
            'tutorial_quick_orientation_basic_navigation_click_text'
          ],
          medium: InteractionMedium.KEYBOARD,
          curriculums: [Curriculum.QUICK_ORIENTATION],
          actions: [
            {
              type: 'key_sequence',
              value: {cvoxModifier: true, keys: {keyCode: [39]}},
            },
            {
              type: 'key_sequence',
              value: {cvoxModifier: true, keys: {keyCode: [32]}},
            }
          ],
          autoInteractive: true,
        },

        {
          title: 'tutorial_quick_orientation_tab_title',
          content: ['tutorial_quick_orientation_tab_text'],
          medium: InteractionMedium.KEYBOARD,
          curriculums: [Curriculum.QUICK_ORIENTATION],
          actions: [{
            type: 'key_sequence',
            value: {keys: {keyCode: [9 /* Tab */]}},
          }],
          autoInteractive: true,
        },

        {
          title: 'tutorial_quick_orientation_shift_tab_title',
          content: ['tutorial_quick_orientation_shift_tab_text'],
          medium: InteractionMedium.KEYBOARD,
          curriculums: [Curriculum.QUICK_ORIENTATION],
          actions: [{
            type: 'key_sequence',
            value: {keys: {keyCode: [9 /* Tab */], shiftKey: [true]}},
          }],
          autoInteractive: true,
        },

        {
          title: 'tutorial_quick_orientation_enter_title',
          content: ['tutorial_quick_orientation_enter_text'],
          medium: InteractionMedium.KEYBOARD,
          curriculums: [Curriculum.QUICK_ORIENTATION],
          actions: [
            {type: 'key_sequence', value: {keys: {keyCode: [13 /* Enter */]}}}
          ],
          autoInteractive: true,
        },

        {
          title: 'tutorial_quick_orientation_lists_title',
          content: [
            'tutorial_quick_orientation_lists_text',
            'tutorial_quick_orientation_lists_continue_text'
          ],
          medium: InteractionMedium.KEYBOARD,
          curriculums: [Curriculum.QUICK_ORIENTATION],
          practiceTitle: 'tutorial_quick_orientation_lists_practice_title',
          practiceInstructions:
              'tutorial_quick_orientation_lists_practice_instructions',
          practiceFile: 'selects',
          practiceState: {},
          events: [],
          hints: []
        },

        {
          title: 'tutorial_quick_orientation_complete_title',
          content: [
            'tutorial_quick_orientation_complete_text',
            'tutorial_quick_orientation_complete_additional_text'
          ],
          medium: InteractionMedium.KEYBOARD,
          curriculums: [Curriculum.QUICK_ORIENTATION],
        },

        {
          title: 'tutorial_on_off_heading',
          content: ['tutorial_control', 'tutorial_on_off'],
          medium: InteractionMedium.KEYBOARD,
          curriculums: [Curriculum.ESSENTIAL_KEYS],
        },

        {
          title: 'tutorial_modifier_heading',
          content: ['tutorial_modifier', 'tutorial_chromebook_search'],
          medium: InteractionMedium.KEYBOARD,
          curriculums: [Curriculum.ESSENTIAL_KEYS],
        },

        {
          title: 'tutorial_basic_navigation_heading',
          content: ['tutorial_basic_navigation'],
          medium: InteractionMedium.KEYBOARD,
          curriculums: [Curriculum.NAVIGATION],
        },

        {
          title: 'tutorial_jump_heading',
          content: [
            'tutorial_jump',
            'tutorial_jump_efficiency',
            'tutorial_jump_more',
          ],
          medium: InteractionMedium.KEYBOARD,
          curriculums: [Curriculum.NAVIGATION],
          practiceTitle: 'tutorial_jump_practice_title',
          practiceInstructions: 'tutorial_jump_practice_instructions',
          practiceFile: 'jump_commands',
          practiceState: {},
          events: [],
          hints: []
        },

        {
          title: 'tutorial_menus_heading',
          content: [
            'tutorial_menus',
          ],
          medium: InteractionMedium.KEYBOARD,
          curriculums: [Curriculum.COMMAND_REFERENCES]
        },

        {
          title: 'tutorial_chrome_shortcuts_heading',
          content: [
            'tutorial_chrome_shortcuts',
            'tutorial_chromebook_ctrl_forward',
          ],
          medium: InteractionMedium.KEYBOARD,
          curriculums: [Curriculum.COMMAND_REFERENCES]
        },

        {
          title: 'tutorial_earcon_page_title',
          content: ['tutorial_earcon_page_body'],
          medium: InteractionMedium.KEYBOARD,
          curriculums: [Curriculum.SOUNDS_AND_SETTINGS]
        },

        {
          title: 'tutorial_learn_more_heading',
          content: ['tutorial_learn_more'],
          medium: InteractionMedium.KEYBOARD,
          curriculums: [Curriculum.RESOURCES],
        }
      ]
    }
  },

  /** @override */
  ready() {
    // Hide screens for now; the tutorial will show when all lessons have
    // finished loading.
    this.hideAllScreens();
    document.addEventListener('keydown', this.onKeyDown.bind(this));
    this.addEventListener('startpractice', (evt) => {
      this.isPracticeAreaActive = true;
      this.startNudges(NudgeType.PRACTICE_AREA);
    });
    this.addEventListener('endpractice', (evt) => {
      this.isPracticeAreaActive = false;
      this.startNudges(NudgeType.GENERAL);
    });
  },

  /** @private */
  onLessonsLoaded_() {
    this.buildEarconLesson();
    this.buildLearnMoreLesson();
    this.dispatchEvent(new CustomEvent('readyfortesting', {composed: true}));
    this.show();
  },

  /** @private */
  onTutorialVisibilityChanged_() {
    if (this.isVisible) {
      this.startNudges(NudgeType.GENERAL);
    } else {
      this.stopNudges();
      this.dispatchEvent(new CustomEvent('closetutorial', {}));
    }
  },

  /** @private */
  onActiveLessonIdChanged_() {
    if (this.activeLessonId < 0 ||
        this.activeLessonId >= this.lessonData.length) {
      return;
    }

    if (this.interactiveMode_) {
      this.stopInteractiveMode();
    }

    const lesson = this.getCurrentLesson();
    if (lesson.autoInteractive) {
      this.startInteractiveMode(lesson.actions);
      // Read the title since initial focus gets placed on the first piece of
      // text content.
      this.readCurrentLessonTitle();
    }
  },

  /** @private */
  onActiveScreenChanged_() {
    if (this.interactiveMode_) {
      this.stopInteractiveMode();
    }
  },

  // Interactive mode.

  /**
   * @param {!Array<{
   *    type: string,
   *    value: (string|Object),
   *    beforeActionMsg: (string|undefined),
   *    afterActionMsg: (string|undefined)}>} actions
   * @private
   */
  startInteractiveMode(actions) {
    this.interactiveMode_ = true;
    this.dispatchEvent(new CustomEvent(
        'startinteractivemode', {composed: true, detail: {actions}}));
  },

  /** @private */
  stopInteractiveMode() {
    this.interactiveMode_ = false;
    this.dispatchEvent(
        new CustomEvent('stopinteractivemode', {composed: true}));
  },

  /**
   * @param {Event} evt
   * @private
   */
  onKeyDown(evt) {
    const key = evt.key;
    if (key === 'Escape') {
      this.exit();
      evt.preventDefault();
      evt.stopPropagation();
      return;
    }

    if (window.BackgroundKeyboardHandler &&
        window.BackgroundKeyboardHandler.onKeyDown) {
      window.BackgroundKeyboardHandler.onKeyDown(evt);
    }

    if (key === 'Tab' && this.interactiveMode_) {
      // Prevent Tab from being used in interactive mode. This ensures the user
      // can only navigate if they press the expected sequence of keys.
      evt.preventDefault();
      evt.stopPropagation();
    }
  },

  // Nudges.

  /**
   * @param {NudgeType} type
   * @private
   */
  startNudges(type) {
    this.stopNudges();
    this.initializeNudges(type);
    this.setNudgeInterval();
  },

  /** @private */
  setNudgeInterval() {
    this.nudgeIntervalId_ =
        setInterval(this.giveNudge.bind(this), this.NUDGE_INTERVAL_TIME_MS_);
  },

  /**
   * @param {NudgeType} type
   * @private
   * @suppress {undefinedVars|missingProperties} For referencing QueueMode,
   * which is defined on the Panel window.
   */
  initializeNudges(type) {
    const maybeGiveNudge = (msg) => {
      if (this.interactiveMode_) {
        // Do not announce message since ChromeVox blocks actions in interactive
        // mode.
        return;
      }

      this.requestSpeech(msg, QueueMode.INTERJECT);
    };

    this.nudgeArray_ = [];
    if (type === NudgeType.PRACTICE_AREA) {
      // Convert hint strings into functions that will request speech for those
      // strings.
      const hints = this.lessonData[this.activeLessonId].hints || [];
      for (const hint of hints) {
        this.nudgeArray_.push(
            this.requestSpeech.bind(this, hint, QueueMode.INTERJECT));
      }
    } else if (type === NudgeType.GENERAL) {
      this.nudgeArray_ = [
        this.requestFullyDescribe.bind(this),
        this.requestFullyDescribe.bind(this),
        this.requestFullyDescribe.bind(this),
        maybeGiveNudge.bind(this, this.getMsg('tutorial_hint_navigate')),
        maybeGiveNudge.bind(this, this.getMsg('tutorial_hint_click')),
        this.requestSpeech.bind(
            this, this.getMsg('tutorial_hint_exit'), QueueMode.INTERJECT)
      ];
    } else {
      throw new Error('Invalid NudgeType: ' + type);
    }
  },

  /** @private */
  stopNudges() {
    this.nudgeCounter_ = 0;
    if (this.nudgeIntervalId_) {
      clearInterval(this.nudgeIntervalId_);
    }
  },

  /** Restarts nudges. */
  restartNudges() {
    this.stopNudges();
    this.setNudgeInterval();
  },

  /** @private */
  giveNudge() {
    if (this.nudgeCounter_ < 0 ||
        this.nudgeCounter_ >= this.nudgeArray_.length) {
      this.stopNudges();
      return;
    }

    this.nudgeArray_[this.nudgeCounter_]();
    this.nudgeCounter_ += 1;
  },

  /**
   * @param {string} text
   * @param {number} queueMode
   * @param {{doNotInterrupt: boolean}=} properties
   * @private
   * @suppress {undefinedVars|missingProperties} For referencing QueueMode,
   * which is defined on the Panel window.
   */
  requestSpeech(text, queueMode, properties) {
    this.dispatchEvent(new CustomEvent(
        'requestspeech',
        {composed: true, detail: {text, queueMode, properties}}));
  },

  /** @private */
  requestFullyDescribe() {
    this.dispatchEvent(
        new CustomEvent('requestfullydescribe', {composed: true}));
  },

  /**
   * @private
   * @suppress {undefinedVars|missingProperties} For referencing QueueMode,
   * which is defined on the Panel window.
   */
  readCurrentLessonTitle() {
    const lesson = this.getCurrentLesson();
    this.requestSpeech(
        this.getMsg(lesson.title), QueueMode.INTERJECT, {doNotInterrupt: true});
  },

  /**
   * @private
   * @suppress {undefinedVars|missingProperties} For referencing QueueMode,
   * which is defined on the Panel window.
   */
  readCurrentLessonContent() {
    const lesson = this.getCurrentLesson();
    for (const text of lesson.content) {
      // Queue lesson content so it is read after the lesson title.
      this.requestSpeech(this.getMsg(text), QueueMode.QUEUE);
    }
  },

  /**
   * @private
   * @suppress {undefinedVars|missingProperties} For referencing
   * EarconDescription, which is defined on the Panel window.
   */
  buildEarconLesson() {
    const earconLesson = this.getLessonWithTitle('tutorial_earcon_page_title');
    if (!earconLesson) {
      throw new Error('Could not find the earcon lesson.');
    }

    // Add text and listeners.
    for (const earconId in EarconDescription) {
      const msgid = EarconDescription[earconId];
      const earconElement = document.createElement('p');
      earconElement.innerText = this.getMsg(msgid);
      earconElement.setAttribute('tabindex', -1);
      earconElement.addEventListener(
          'focus', this.requestEarcon.bind(this, earconId));
      earconLesson.contentDiv.appendChild(earconElement);
    }
  },

  /**
   * @param {string} earconId
   * @private
   */
  requestEarcon(earconId) {
    this.dispatchEvent(
        new CustomEvent('requestearcon', {composed: true, detail: {earconId}}));
  },

  /** @private */
  buildLearnMoreLesson() {
    const learnMoreLesson =
        this.getLessonWithTitle('tutorial_learn_more_heading');
    if (!learnMoreLesson) {
      throw new Error('Could not find the learn more lesson');
    }

    // Add links to resources.
    const resources = [
      {
        msgId: 'next_command_reference',
        link: 'https://www.chromevox.com/next_keyboard_shortcuts.html'
      },
      {
        msgId: 'chrome_keyboard_shortcuts',
        link: 'https://support.google.com/chromebook/answer/183101?hl=en'
      },
      {
        msgId: 'touchscreen_accessibility',
        link: 'https://support.google.com/chromebook/answer/6103702?hl=en'
      },
    ];
    for (const resource of resources) {
      const link = document.createElement('a');
      link.innerText = this.getMsg(resource.msgId);
      link.href = resource.link;
      link.addEventListener('click', (evt) => {
        this.stopNudges();
        this.dispatchEvent(new CustomEvent(
            'openUrl', {composed: true, detail: {url: link.href}}));
        evt.preventDefault();
        evt.stopPropagation();
      });
      learnMoreLesson.contentDiv.appendChild(link);
      const br = document.createElement('br');
      learnMoreLesson.contentDiv.appendChild(br);
    }
  },
});
