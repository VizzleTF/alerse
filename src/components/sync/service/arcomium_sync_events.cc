// Copyright 2026 The Arcomium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/arcomium_sync_events.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/strings/escape.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace syncer {

namespace {

constexpr base::TimeDelta kFirstReconnectDelay = base::Seconds(5);
constexpr base::TimeDelta kMaxReconnectDelay = base::Minutes(5);
// A line longer than this is not an event of the server's.
constexpr size_t kMaxLineLength = 64 * 1024;

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("arcomium_sync_events", R"(
      semantics {
        sender: "Arcomium Sync"
        description:
          "Keeps a stream open to the Arcomium sync server the user chose, "
          "which says when another device of the user's sync chain changed "
          "data, so this device fetches the change right away."
        trigger: "Sync with an Arcomium server is set up and running."
        data:
          "A token signed with the sync chain's key and this device's sync "
          "cache ID. No browsing data is sent."
        destination: OTHER
        destination_other: "The Arcomium sync server the user set up."
      }
      policy {
        cookies_allowed: NO
        setting: "Turn off Arcomium sync in settings."
        policy_exception_justification: "Not implemented."
      })");

GURL MakeEventsURL(const GURL& server_url, const std::string& cache_guid) {
  std::string spec = server_url.spec();
  if (!spec.ends_with('/')) {
    spec += '/';
  }
  return GURL(spec + "events?cache_guid=" +
              base::EscapeQueryParamValue(cache_guid, /*use_plus=*/true));
}

}  // namespace

ArcomiumSyncEvents::ArcomiumSyncEvents(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const GURL& server_url,
    const std::string& cache_guid,
    TokenGetter token_getter,
    Refresh refresh)
    : url_loader_factory_(std::move(url_loader_factory)),
      events_url_(MakeEventsURL(server_url, cache_guid)),
      token_getter_(std::move(token_getter)),
      refresh_(std::move(refresh)),
      reconnect_delay_(kFirstReconnectDelay) {
  Connect();
}

ArcomiumSyncEvents::~ArcomiumSyncEvents() = default;

void ArcomiumSyncEvents::Connect() {
  pending_.clear();
  event_name_.clear();
  event_data_.clear();

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = events_url_;
  request->method = net::HttpRequestHeaders::kGetMethod;
  request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  request->load_flags = net::LOAD_DISABLE_CACHE;
  request->headers.SetHeader(net::HttpRequestHeaders::kAuthorization,
                             "Bearer " + token_getter_.Run());
  request->headers.SetHeader(net::HttpRequestHeaders::kAccept,
                             "text/event-stream");

  loader_ = network::SimpleURLLoader::Create(std::move(request),
                                             kTrafficAnnotation);
  loader_->DownloadAsStream(url_loader_factory_.get(), this);
}

void ArcomiumSyncEvents::OnDataReceived(std::string_view data,
                                        base::OnceClosure resume) {
  // Data means the server accepted the stream.
  reconnect_delay_ = kFirstReconnectDelay;
  pending_.append(data);
  size_t start = 0;
  for (size_t end; (end = pending_.find('\n', start)) != std::string::npos;
       start = end + 1) {
    std::string_view line =
        std::string_view(pending_).substr(start, end - start);
    if (line.ends_with('\r')) {
      line.remove_suffix(1);
    }
    HandleLine(line);
  }
  pending_.erase(0, start);
  if (pending_.size() > kMaxLineLength) {
    pending_.clear();
  }
  std::move(resume).Run();
}

void ArcomiumSyncEvents::OnComplete(bool success) {
  // The stream should not end; when it does, connect again, waiting longer
  // each time it fails before any data.
  loader_.reset();
  reconnect_timer_.Start(FROM_HERE, reconnect_delay_,
                         base::BindOnce(&ArcomiumSyncEvents::Connect,
                                        base::Unretained(this)));
  reconnect_delay_ = std::min(reconnect_delay_ * 2, kMaxReconnectDelay);
}

void ArcomiumSyncEvents::OnRetry(base::OnceClosure start_retry) {
  // Retries are not enabled.
  NOTREACHED();
}

void ArcomiumSyncEvents::HandleLine(std::string_view line) {
  if (line.empty()) {
    DispatchEvent();
    return;
  }
  if (line.starts_with(':')) {
    return;  // A comment, such as the server's keep-alive ping.
  }
  if (line.starts_with("event:")) {
    event_name_ = std::string(
        base::TrimWhitespaceASCII(line.substr(6), base::TRIM_LEADING));
  } else if (line.starts_with("data:")) {
    event_data_ = std::string(
        base::TrimWhitespaceASCII(line.substr(5), base::TRIM_LEADING));
  }
}

void ArcomiumSyncEvents::DispatchEvent() {
  const std::string name = std::exchange(event_name_, std::string());
  const std::string data = std::exchange(event_data_, std::string());

  if (name == "ready" || name == "resync") {
    refresh_.Run(ProtocolTypes());
    return;
  }
  if (name != "changed") {
    return;
  }
  std::optional<base::DictValue> event = base::JSONReader::ReadDict(data, base::JSON_PARSE_RFC);
  const base::ListValue* numbers = event ? event->FindList("types") : nullptr;
  if (!numbers) {
    return;
  }
  DataTypeSet types;
  for (const base::Value& number : *numbers) {
    if (number.is_int()) {
      const DataType type = GetDataTypeFromSpecificsFieldNumber(number.GetInt());
      if (type != UNSPECIFIED) {
        types.Put(type);
      }
    }
  }
  if (!types.empty()) {
    refresh_.Run(types);
  }
}

}  // namespace syncer
