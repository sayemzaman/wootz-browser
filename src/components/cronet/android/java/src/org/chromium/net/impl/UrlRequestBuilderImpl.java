// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.net.impl;

import android.os.Build;
import android.util.Log;

import org.chromium.net.CronetEngine;
import org.chromium.net.ExperimentalUrlRequest;
import org.chromium.net.RequestFinishedInfo;
import org.chromium.net.UploadDataProvider;
import org.chromium.net.UrlRequest;

import java.util.AbstractMap;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Map;
import java.util.Objects;
import java.util.concurrent.Executor;

/** Implements {@link org.chromium.net.ExperimentalUrlRequest.Builder}. */
public class UrlRequestBuilderImpl extends ExperimentalUrlRequest.Builder {
    private static final String ACCEPT_ENCODING = "Accept-Encoding";
    private static final String TAG = UrlRequestBuilderImpl.class.getSimpleName();

    // All fields are temporary storage of ExperimentalUrlRequest configuration to be
    // copied to built ExperimentalUrlRequest.

    // CronetEngineBase to execute request.
    private final CronetEngineBase mCronetEngine;
    // URL to request.
    private final String mUrl;
    // Callback to receive progress callbacks.
    private final UrlRequest.Callback mCallback;
    // Executor to invoke callback on.
    private final Executor mExecutor;
    // HTTP method (e.g. GET, POST etc).
    private String mMethod;

    // List of request headers, stored as header field name and value map entries.
    private final ArrayList<Map.Entry<String, String>> mRequestHeaders = new ArrayList<>();
    // Disable the cache for just this request.
    private boolean mDisableCache;
    // Disable connection migration for just this request.
    private boolean mDisableConnectionMigration;
    // Priority of request. Default is medium.
    @CronetEngineBase.RequestPriority private int mPriority = REQUEST_PRIORITY_MEDIUM;
    // Request reporting annotations. Avoid extra object creation if no annotations added.
    private Collection<Object> mRequestAnnotations;
    // If request is an upload, this provides the request body data.
    private UploadDataProvider mUploadDataProvider;
    // Executor to call upload data provider back on.
    private Executor mUploadDataProviderExecutor;
    private boolean mAllowDirectExecutor;
    private boolean mTrafficStatsTagSet;
    private int mTrafficStatsTag;
    private boolean mTrafficStatsUidSet;
    private int mTrafficStatsUid;
    private RequestFinishedInfo.Listener mRequestFinishedListener;
    private long mNetworkHandle = CronetEngineBase.DEFAULT_NETWORK_HANDLE;
    // Idempotency of the request.
    @CronetEngineBase.Idempotency private int mIdempotency = DEFAULT_IDEMPOTENCY;

    /**
     * Creates a builder for {@link UrlRequest} objects. All callbacks for generated {@link
     * UrlRequest} objects will be invoked on {@code executor}'s thread. {@code executor} must not
     * run tasks on the current thread to prevent blocking networking operations and causing
     * exceptions during shutdown.
     *
     * @param url URL for the generated requests.
     * @param callback callback object that gets invoked on different events.
     * @param executor {@link Executor} on which all callbacks will be invoked.
     * @param cronetEngine {@link CronetEngine} used to execute this request.
     */
    UrlRequestBuilderImpl(
            String url,
            UrlRequest.Callback callback,
            Executor executor,
            CronetEngineBase cronetEngine) {
        super();
        mUrl = Objects.requireNonNull(url, "URL is required.");
        mCallback = Objects.requireNonNull(callback, "Callback is required.");
        mExecutor = Objects.requireNonNull(executor, "Executor is required.");
        mCronetEngine = Objects.requireNonNull(cronetEngine, "CronetEngine is required.");
    }

    @Override
    public ExperimentalUrlRequest.Builder setHttpMethod(String method) {
        mMethod = Objects.requireNonNull(method, "Method is required.");
        return this;
    }

    @Override
    public UrlRequestBuilderImpl addHeader(String header, String value) {
        Objects.requireNonNull(header, "Invalid header name.");
        Objects.requireNonNull(value, "Invalid header value.");
        if (ACCEPT_ENCODING.equalsIgnoreCase(header)) {
            if (Log.isLoggable(TAG, Log.DEBUG)) {
                Log.d(
                        TAG,
                        "It's not necessary to set Accept-Encoding on requests - cronet will do"
                                + " this automatically for you, and setting it yourself has no "
                                + "effect. See https://crbug.com/581399 for details.",
                        new Exception());
            }
            return this;
        }
        mRequestHeaders.add(new AbstractMap.SimpleEntry<>(header, value));
        return this;
    }

    @Override
    public UrlRequestBuilderImpl disableCache() {
        mDisableCache = true;
        return this;
    }

    @Override
    public UrlRequestBuilderImpl disableConnectionMigration() {
        mDisableConnectionMigration = true;
        return this;
    }

    @Override
    public UrlRequestBuilderImpl setPriority(@CronetEngineBase.RequestPriority int priority) {
        mPriority = priority;
        return this;
    }

    @Override
    public UrlRequestBuilderImpl setIdempotency(@CronetEngineBase.Idempotency int idempotency) {
        mIdempotency = idempotency;
        return this;
    }

    @Override
    public UrlRequestBuilderImpl setUploadDataProvider(
            UploadDataProvider uploadDataProvider, Executor executor) {
        mUploadDataProvider =
                Objects.requireNonNull(uploadDataProvider, "Invalid UploadDataProvider.");
        mUploadDataProviderExecutor =
                Objects.requireNonNull(executor, "Invalid UploadDataProvider Executor.");
        if (mMethod == null) {
            mMethod = "POST";
        }
        return this;
    }

    @Override
    public UrlRequestBuilderImpl allowDirectExecutor() {
        mAllowDirectExecutor = true;
        return this;
    }

    @Override
    public UrlRequestBuilderImpl addRequestAnnotation(Object annotation) {
        Objects.requireNonNull(annotation, "Invalid metrics annotation.");
        if (mRequestAnnotations == null) {
            mRequestAnnotations = new ArrayList<>();
        }
        mRequestAnnotations.add(annotation);
        return this;
    }

    @Override
    public UrlRequestBuilderImpl setTrafficStatsTag(int tag) {
        mTrafficStatsTagSet = true;
        mTrafficStatsTag = tag;
        return this;
    }

    @Override
    public UrlRequestBuilderImpl setTrafficStatsUid(int uid) {
        mTrafficStatsUidSet = true;
        mTrafficStatsUid = uid;
        return this;
    }

    @Override
    public UrlRequestBuilderImpl setRequestFinishedListener(RequestFinishedInfo.Listener listener) {
        mRequestFinishedListener = listener;
        return this;
    }

    @Override
    public UrlRequestBuilderImpl bindToNetwork(long networkHandle) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) {
            throw new UnsupportedOperationException(
                    "The multi-network API is available starting from Android Marshmallow");
        }
        mNetworkHandle = networkHandle;
        return this;
    }

    @Override
    public ExperimentalUrlRequest build() {
        // @SuppressLint("WrongConstant") // TODO(jbudorick): Remove this after rolling to the N
        // SDK.
        return mCronetEngine.createRequest(
                mUrl,
                mCallback,
                mExecutor,
                mPriority,
                mRequestAnnotations,
                mDisableCache,
                mDisableConnectionMigration,
                mAllowDirectExecutor,
                mTrafficStatsTagSet,
                mTrafficStatsTag,
                mTrafficStatsUidSet,
                mTrafficStatsUid,
                mRequestFinishedListener,
                mIdempotency,
                mNetworkHandle,
                mMethod == null ? "GET" : mMethod, // default to get if not set.
                mRequestHeaders,
                mUploadDataProvider,
                mUploadDataProviderExecutor);
    }
}
