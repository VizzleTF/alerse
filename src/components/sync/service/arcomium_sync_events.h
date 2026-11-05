// Copyright 2026 The Arcomium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SERVICE_ARCOMIUM_SYNC_EVENTS_H_
#define COMPONENTS_SYNC_SERVICE_ARCOMIUM_SYNC_EVENTS_H_

#include <memory>
#include <string>
#include <string_view>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/sync/base/data_type.h"
#include "services/network/public/cpp/simple_url_loader_stream_consumer.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace syncer {

// Keeps the Arcomium sync server's event stream (GET <server>/events) open, so
// a commit on another device of the chain is fetched right away instead of at
// the next poll. The stream is Server-Sent Events:
//
//   event: ready     the stream is open: fetch what may have been missed
//   event: changed   data: {"types":[32904]}: fetch these data types
//   event: resync    the server lost events: fetch everything
//
// It reconnects with growing delays when the stream ends.
class ArcomiumSyncEvents : public network::SimpleURLLoaderStreamConsumer {
 public:
  // Returns a fresh token for the request.
  using TokenGetter = base::RepeatingCallback<std::string()>;
  // Fetches updates of these data types.
  using Refresh = base::RepeatingCallback<void(const DataTypeSet&)>;

  ArcomiumSyncEvents(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& server_url,
      const std::string& cache_guid,
      TokenGetter token_getter,
      Refresh refresh);
  ArcomiumSyncEvents(const ArcomiumSyncEvents&) = delete;
  ArcomiumSyncEvents& operator=(const ArcomiumSyncEvents&) = delete;
  ~ArcomiumSyncEvents() override;

  // network::SimpleURLLoaderStreamConsumer:
  void OnDataReceived(std::string_view data, base::OnceClosure resume) override;
  void OnComplete(bool success) override;
  void OnRetry(base::OnceClosure start_retry) override;

 private:
  void Connect();
  void HandleLine(std::string_view line);
  void DispatchEvent();

  const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  const GURL events_url_;
  const TokenGetter token_getter_;
  const Refresh refresh_;

  std::unique_ptr<network::SimpleURLLoader> loader_;
  // Text after the last full line, and the event being read.
  std::string pending_;
  std::string event_name_;
  std::string event_data_;

  base::OneShotTimer reconnect_timer_;
  base::TimeDelta reconnect_delay_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_SERVICE_ARCOMIUM_SYNC_EVENTS_H_
