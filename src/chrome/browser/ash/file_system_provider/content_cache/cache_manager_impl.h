// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_CONTENT_CACHE_CACHE_MANAGER_IMPL_H_
#define CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_CONTENT_CACHE_CACHE_MANAGER_IMPL_H_

#include <set>

#include "base/files/file.h"
#include "base/files/file_error_or.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/file_system_provider/abort_callback.h"
#include "chrome/browser/ash/file_system_provider/content_cache/cache_manager.h"
#include "chrome/browser/ash/file_system_provider/content_cache/content_cache.h"
#include "chrome/browser/ash/file_system_provider/content_cache/context_database.h"
#include "chromeos/ash/components/dbus/spaced/spaced_client.h"

namespace ash::file_system_provider {

// The max size of each individual content cache in bytes, i.e. 5GB.
inline constexpr int64_t kMaxAllowedCacheSizeInBytes = int64_t(5) << 30;

class CacheManagerImpl : public CacheManager,
                         public ash::SpacedClient::Observer {
 public:
  explicit CacheManagerImpl(const base::FilePath& profile_path);

  CacheManagerImpl(const CacheManagerImpl&) = delete;
  CacheManagerImpl& operator=(const CacheManagerImpl&) = delete;

  ~CacheManagerImpl() override;

  static std::unique_ptr<CacheManager> Create(
      const base::FilePath& profile_path);

  // Setup the cache directory for the specific FSP.
  void InitializeForProvider(const ProvidedFileSystemInfo& file_system_info,
                             FileErrorOrContentCacheCallback callback) override;

  void UninitializeForProvider(
      const ProvidedFileSystemInfo& file_system_info) override;

  bool IsProviderInitialized(
      const ProvidedFileSystemInfo& file_system_info) override;

  void AddObserver(CacheManager::Observer* observer) override;
  void RemoveObserver(CacheManager::Observer* observer) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(FileSystemProviderCacheManagerImplTest,
                           OnSpaceUpdate);

  // Attempt to initialize the context database if the directory creation was
  // successful of the in_memory_only boolean is true.
  void OnProviderDirectoryCreationComplete(
      FileErrorOrContentCacheCallback callback,
      base::FilePath cache_directory_path,
      base::File::Error result);

  // Called once the deletion of the cache directory has been attempted.
  void OnUninitializeForProvider(
      const base::FilePath& base64_encoded_provider_folder_name,
      base::File::Error result);

  const base::FilePath GetCacheDirectoryPath(
      const ProvidedFileSystemInfo& file_system_info);

  // Called when the `ContextDatabase` has been setup: either a new database has
  // been created OR the old database has been connected to.
  void OnProviderContextDatabaseSetup(
      const base::FilePath& cache_directory_path,
      FileErrorOrContentCacheCallback callback,
      scoped_refptr<base::SequencedTaskRunner> db_task_runner,
      OptionalContextDatabase optional_context_db);

  void OnProviderFilesLoadedFromDisk(
      std::unique_ptr<ContentCache> content_cache,
      const base::FilePath& base64_encoded_provider_folder_name,
      FileErrorOrContentCacheCallback callback);

  // When the initialization has finished, invoke the callback and notify the
  // observers.
  void OnProviderInitializationComplete(
      const base::FilePath& base64_encoded_provider_folder_name,
      FileErrorOrContentCacheCallback callback,
      FileErrorOrContentCache error_or_content_cache);

  // Retrieves free space (either on-demand or via the spaced observer) and
  // updates the max available size for every initialized content cache.
  void GetFreeSpaceAndResizeInitializedCaches();
  void OnFreeSpace(int64_t free_space_in_bytes);

  // ash::SpacedClient overrides.
  void OnSpaceUpdate(const SpaceEvent& event) override;

  const base::FilePath root_content_cache_directory_;

  // A map (keyed by a base64 encoded provider mount path) of weak pointers to
  // content caches.
  std::map<base::FilePath, base::WeakPtr<ContentCache>> initialized_caches_;
  base::ObserverList<CacheManager::Observer> observers_;

  base::ScopedObservation<ash::SpacedClient, ash::SpacedClient::Observer>
      spaced_client_{this};

  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_{
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(),
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})};

  base::WeakPtrFactory<CacheManagerImpl> weak_ptr_factory_{this};
};

}  // namespace ash::file_system_provider

#endif  // CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_CONTENT_CACHE_CACHE_MANAGER_IMPL_H_
