// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef QUICHE_QUIC_TOOLS_QUIC_SIMPLE_SERVER_BACKEND_H_
#define QUICHE_QUIC_TOOLS_QUIC_SIMPLE_SERVER_BACKEND_H_

#include <memory>

#include "quic/core/quic_types.h"
#include "quic/core/web_transport_interface.h"
#include "quic/tools/quic_backend_response.h"
#include "spdy/core/spdy_header_block.h"

namespace quic {

// This interface implements the functionality to fetch a response
// from the backend (such as cache, http-proxy etc) to serve
// requests received by a Quic Server
class QuicSimpleServerBackend {
 public:
  // This interface implements the methods
  // called by the QuicSimpleServerBackend implementation
  // to process the request in the backend
  class RequestHandler {
   public:
    virtual ~RequestHandler() {}

    virtual QuicConnectionId connection_id() const = 0;
    virtual QuicStreamId stream_id() const = 0;
    virtual std::string peer_host() const = 0;
    // Called when the response is ready at the backend and can be send back to
    // the QUIC client.
    virtual void OnResponseBackendComplete(
        const QuicBackendResponse* response) = 0;
  };

  struct WebTransportResponse {
    spdy::Http2HeaderBlock response_headers;
    std::unique_ptr<WebTransportVisitor> visitor;
  };

  virtual ~QuicSimpleServerBackend() = default;
  // This method initializes the backend instance to fetch responses
  // from a backend server, in-memory cache etc.
  virtual bool InitializeBackend(const std::string& backend_url) = 0;
  // Returns true if the backend has been successfully initialized
  // and could be used to fetch HTTP requests
  virtual bool IsBackendInitialized() const = 0;
  // Triggers a HTTP request to be sent to the backend server or cache
  // If response is immediately available, the function synchronously calls
  // the |request_handler| with the HTTP response.
  // If the response has to be fetched over the network, the function
  // asynchronously calls |request_handler| with the HTTP response.
  virtual void FetchResponseFromBackend(
      const spdy::Http2HeaderBlock& request_headers,
      const std::string& request_body,
      RequestHandler* request_handler) = 0;
  // Clears the state of the backend  instance
  virtual void CloseBackendResponseStream(RequestHandler* request_handler) = 0;

  virtual WebTransportResponse ProcessWebTransportRequest(
      const spdy::Http2HeaderBlock& /*request_headers*/,
      WebTransportSession* /*session*/) {
    WebTransportResponse response;
    response.response_headers[":status"] = "400";
    return response;
  }
  virtual bool SupportsWebTransport() { return false; }
  virtual bool UsesDatagramContexts() { return false; }
  virtual bool SupportsExtendedConnect() { return true; }
};

}  // namespace quic

#endif  // QUICHE_QUIC_TOOLS_QUIC_SIMPLE_SERVER_BACKEND_H_
