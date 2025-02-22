// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/managed_footnote/managed_footnote.js';
import './item.js';
import './mv2_deprecation_panel.js';
import './shared_style.css.js';
import './review_panel.js';

import {getInstance as getAnnouncerInstance} from 'chrome://resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {ExtensionsItemElement, ItemDelegate} from './item.js';
import {getTemplate} from './item_list.html.js';

type Filter = (info: chrome.developerPrivate.ExtensionInfo) => boolean;

const ExtensionsItemListElementBase = I18nMixin(PolymerElement);

export class ExtensionsItemListElement extends ExtensionsItemListElementBase {
  static get is() {
    return 'extensions-item-list';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      apps: Array,
      extensions: Array,
      delegate: Object,

      inDevMode: {
        type: Boolean,
        value: false,
      },

      isMv2DeprecationWarningDismissed: {
        type: Boolean,
        notify: true,
      },

      filter: {
        type: String,
      },

      computedFilter_: {
        type: String,
        computed: 'computeFilter_(filter)',
        observer: 'announceSearchResults_',
      },

      maxColumns_: {
        type: Number,
        value: 3,
      },

      /**
       * List of extensions that are affected by the mv2 deprecation and should
       * be visible in the mv2 deprecation panel.
       */
      mv2DeprecatedExtensions_: {
        type: Array,
        computed: 'computeMv2DeprecatedExtensions_(extensions.*)',
      },

      shownAppsCount_: {
        type: Number,
        value: 0,
      },

      shownExtensionsCount_: {
        type: Number,
        value: 0,
      },

      showSafetyCheckReviewPanel_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('safetyCheckShowReviewPanel') ||
            loadTimeData.getBoolean('safetyHubShowReviewPanel'),
      },

      /*
       * Indicates whether the mv2 deprecation panel is shown.
       */
      showMv2DeprecationPanel_: {
        type: Boolean,
        computed: 'computeShowMv2DeprecationPanel_(' +
            'isMv2DeprecationWarningDismissed, mv2DeprecatedExtensions_)',
      },

      hasSafetyCheckTriggeringExtension_: {
        type: Boolean,
        computed: 'computeHasSafetyCheckTriggeringExtension_(extensions)',
      },
    };
  }

  apps: chrome.developerPrivate.ExtensionInfo[];
  extensions: chrome.developerPrivate.ExtensionInfo[];
  delegate: ItemDelegate;
  inDevMode: boolean;
  isMv2DeprecationWarningDismissed: boolean;
  filter: string;
  private computedFilter_: string;
  private maxColumns_: number;
  private mv2DeprecatedExtensions_: chrome.developerPrivate.ExtensionInfo[];
  private shownAppsCount_: number;
  private shownExtensionsCount_: number;
  private showMv2DeprecationPanel_: boolean;
  private showSafetyCheckReviewPanel_: boolean;
  private hasSafetyCheckTriggeringExtension_: boolean;

  getDetailsButton(id: string): HTMLElement|null {
    const item =
        this.shadowRoot!.querySelector<ExtensionsItemElement>(`#${id}`);
    return item && item.getDetailsButton();
  }

  getRemoveButton(id: string): HTMLElement|null {
    const item =
        this.shadowRoot!.querySelector<ExtensionsItemElement>(`#${id}`);
    return item && item.getRemoveButton();
  }

  getErrorsButton(id: string): HTMLElement|null {
    const item =
        this.shadowRoot!.querySelector<ExtensionsItemElement>(`#${id}`);
    return item && item.getErrorsButton();
  }

  /**
   * Focus the remove button for the item matching `id`. If the remove button is
   * not visible, focus the details button instead.
   * return: If an item's button has been focused, see comment below.
   */
  focusItemButton(id: string): boolean {
    const item =
        this.shadowRoot!.querySelector<ExtensionsItemElement>(`#${id}`);
    // This function is called from a setTimeout() inside manager.ts. Rarely,
    // the list of extensions rendered in this element may not match the list of
    // extensions stored in manager.ts for a brief moment (not visible to the
    // user). As a result, `item` here may be null even though `id` points to
    // an extension inside `manager.ts`. If this happens, do not focus anything.
    // Observed in crbug.com/1482580.
    if (!item) {
      return false;
    }

    const buttonToFocus = item.getRemoveButton() || item.getDetailsButton();
    buttonToFocus!.focus();
    return true;
  }

  /**
   * Computes the filter function to be used for determining which items
   * should be shown. A |null| value indicates that everything should be
   * shown.
   * return {?Function}
   */
  private computeFilter_(): Filter|null {
    const formattedFilter = this.filter.trim().toLowerCase();
    if (!formattedFilter) {
      return null;
    }

    return i => [i.name, i.id].some(
               s => s.toLowerCase().includes(formattedFilter));
  }

  /**
   * Computes the extensions that are affected by the manifest v2 deprecation
   * and should be visible in the MV2 deprecation panel.
   */
  private computeMv2DeprecatedExtensions_():
      chrome.developerPrivate.ExtensionInfo[] {
    return this.extensions.filter((extension) => {
      return extension.isAffectedByMV2Deprecation &&
          !extension.didAcknowledgeMV2DeprecationWarning;
    });
  }

  private computeShowSafetyCheckReviewPanel_(): boolean {
    return (
        loadTimeData.getBoolean('safetyCheckShowReviewPanel') ||
        loadTimeData.getBoolean('safetyHubShowReviewPanel'));
  }

  private computeHasSafetyCheckTriggeringExtension_(): boolean {
    if (!this.extensions) {
      return false;
    }
    for (const extension of this.extensions) {
      if (!!extension.safetyCheckText &&
          !!extension.safetyCheckText.panelString &&
          this.showSafetyCheckReviewPanel_) {
        return true;
      }
    }
    return false;
  }

  /**
   * Returns whether the manifest v2 deprecation panel should be visible.
   */
  private computeShowMv2DeprecationPanel_(): boolean {
    // Panel is visible iff it should be enabled, hasn't been dismissed by the
    // user and at least one extension is affected by the MV2 deprecation.
    // Note: deprecated extensions may not be initialized at construction, thus
    // we check for their existence.
    return loadTimeData.getBoolean('MV2DeprecationPanelEnabled') &&
        !this.isMv2DeprecationWarningDismissed &&
        this.mv2DeprecatedExtensions_?.length !== 0;
  }

  private shouldShowEmptyItemsMessage_() {
    if (!this.apps || !this.extensions) {
      return;
    }

    return this.apps.length === 0 && this.extensions.length === 0;
  }

  private shouldShowEmptySearchMessage_() {
    return !this.shouldShowEmptyItemsMessage_() && this.shownAppsCount_ === 0 &&
        this.shownExtensionsCount_ === 0;
  }

  private onNoExtensionsClick_(e: Event) {
    if ((e.target as HTMLElement).tagName === 'A') {
      chrome.metricsPrivate.recordUserAction('Options_GetMoreExtensions');
    }
  }

  private announceSearchResults_() {
    if (this.computedFilter_) {
      setTimeout(() => {  // Async to allow list to update.
        const total = this.shownAppsCount_ + this.shownExtensionsCount_;
        getAnnouncerInstance().announce(this.shouldShowEmptySearchMessage_() ?
            this.i18n('noSearchResults') :
            (total === 1 ?
                 this.i18n('searchResultsSingular', this.filter) :
                 this.i18n(
                     'searchResultsPlural', total.toString(), this.filter)));
      }, 0);
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'extensions-item-list': ExtensionsItemListElement;
  }
}

customElements.define(ExtensionsItemListElement.is, ExtensionsItemListElement);
