// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_NEW_TAB_PAGE_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_NEW_TAB_PAGE_PAGE_LOAD_METRICS_OBSERVER_H_

#include "components/page_load_metrics/browser/page_load_metrics_observer.h"

// NewTabPagePageLoadMetricsObserver is responsible for recording performance
// related metrics, such as NavigationOrActivationToFirstContentfulPaint and
// NavigationOrActivationToLargestContentfulPaint for NewTabPage navigations.
class NewTabPagePageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  NewTabPagePageLoadMetricsObserver();

  NewTabPagePageLoadMetricsObserver(const NewTabPagePageLoadMetricsObserver&) =
      delete;
  NewTabPagePageLoadMetricsObserver& operator=(
      const NewTabPagePageLoadMetricsObserver&) = delete;

  ~NewTabPagePageLoadMetricsObserver() override;

 private:
  // page_load_metrics::PageLoadMetricsObserver:
  ObservePolicy OnStart(content::NavigationHandle* navigation_handle,
                        const GURL& currently_committed_url,
                        bool started_in_foreground) override;
  ObservePolicy OnPrerenderStart(content::NavigationHandle* navigation_handle,
                                 const GURL& currently_committed_url) override;
  ObservePolicy OnFencedFramesStart(
      content::NavigationHandle* navigation_handle,
      const GURL& currently_committed_url) override;
  void DidActivatePrerenderedPage(
      content::NavigationHandle* navigation_handle) override;
  ObservePolicy OnCommit(content::NavigationHandle* navigation_handle) override;
  void OnFirstContentfulPaintInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnFirstMeaningfulPaintInMainFrameDocument(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  ObservePolicy FlushMetricsOnAppEnterBackground(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnComplete(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;

 private:
  void RecordSessionEndHistograms(
      const page_load_metrics::mojom::PageLoadTiming& timing);
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_NEW_TAB_PAGE_PAGE_LOAD_METRICS_OBSERVER_H_
