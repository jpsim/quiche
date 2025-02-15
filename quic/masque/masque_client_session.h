// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef QUICHE_QUIC_MASQUE_MASQUE_CLIENT_SESSION_H_
#define QUICHE_QUIC_MASQUE_MASQUE_CLIENT_SESSION_H_

#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "quic/core/http/quic_spdy_client_session.h"
#include "quic/masque/masque_compression_engine.h"
#include "quic/masque/masque_utils.h"
#include "quic/platform/api/quic_export.h"
#include "quic/platform/api/quic_socket_address.h"

namespace quic {

// QUIC client session for connection to MASQUE proxy. This session establishes
// a connection to a MASQUE proxy and handles sending and receiving DATAGRAM
// frames for operation of the MASQUE protocol. Multiple end-to-end encapsulated
// sessions can then coexist inside this session. Once these are created, they
// need to be registered with this session.
class QUIC_NO_EXPORT MasqueClientSession : public QuicSpdyClientSession {
 public:
  // Interface meant to be implemented by the owner of the
  // MasqueClientSession instance.
  class QUIC_NO_EXPORT Owner {
   public:
    virtual ~Owner() {}

    // Notifies the owner that the client connection ID is no longer in use.
    virtual void UnregisterClientConnectionId(
        QuicConnectionId client_connection_id) = 0;

    // Notifies the owner that a settings frame has been received.
    virtual void OnSettingsReceived() = 0;
  };
  // Interface meant to be implemented by encapsulated client sessions, i.e.
  // the end-to-end QUIC client sessions that run inside MASQUE encapsulation.
  class QUIC_NO_EXPORT EncapsulatedClientSession {
   public:
    virtual ~EncapsulatedClientSession() {}

    // Process packet that was just decapsulated.
    virtual void ProcessPacket(absl::string_view packet,
                               QuicSocketAddress target_server_address) = 0;

    // Close the encapsulated connection.
    virtual void CloseConnection(
        QuicErrorCode error,
        const std::string& details,
        ConnectionCloseBehavior connection_close_behavior) = 0;
  };

  // Takes ownership of |connection|, but not of |crypto_config| or
  // |push_promise_index| or |owner|. All pointers must be non-null. Caller
  // must ensure that |push_promise_index| and |owner| stay valid for the
  // lifetime of the newly created MasqueClientSession.
  MasqueClientSession(MasqueMode masque_mode, const std::string& uri_template,
                      const QuicConfig& config,
                      const ParsedQuicVersionVector& supported_versions,
                      QuicConnection* connection, const QuicServerId& server_id,
                      QuicCryptoClientConfig* crypto_config,
                      QuicClientPushPromiseIndex* push_promise_index,
                      Owner* owner);

  // Disallow copy and assign.
  MasqueClientSession(const MasqueClientSession&) = delete;
  MasqueClientSession& operator=(const MasqueClientSession&) = delete;

  // From QuicSession.
  void OnMessageReceived(absl::string_view message) override;
  void OnMessageAcked(QuicMessageId message_id,
                      QuicTime receive_timestamp) override;
  void OnMessageLost(QuicMessageId message_id) override;
  void OnConnectionClosed(const QuicConnectionCloseFrame& frame,
                          ConnectionCloseSource source) override;
  void OnStreamClosed(QuicStreamId stream_id) override;

  // From QuicSpdySession.
  bool OnSettingsFrame(const SettingsFrame& frame) override;

  // Send encapsulated packet.
  void SendPacket(QuicConnectionId client_connection_id,
                  QuicConnectionId server_connection_id,
                  absl::string_view packet,
                  const QuicSocketAddress& target_server_address,
                  EncapsulatedClientSession* encapsulated_client_session);

  // Register encapsulated client. This allows clients that are encapsulated
  // within this MASQUE session to indicate they own a given client connection
  // ID so incoming packets with that connection ID are routed back to them.
  // Callers must not register a second different |encapsulated_client_session|
  // with the same |client_connection_id|. Every call must be matched with a
  // call to UnregisterConnectionId.
  void RegisterConnectionId(
      QuicConnectionId client_connection_id,
      EncapsulatedClientSession* encapsulated_client_session);

  // Unregister encapsulated client. |client_connection_id| must match a
  // value previously passed to RegisterConnectionId.
  void UnregisterConnectionId(
      QuicConnectionId client_connection_id,
      EncapsulatedClientSession* encapsulated_client_session);

 private:
  // State that the MasqueClientSession keeps for each CONNECT-UDP request.
  class QUIC_NO_EXPORT ConnectUdpClientState
      : public QuicSpdyStream::Http3DatagramRegistrationVisitor,
        public QuicSpdyStream::Http3DatagramVisitor {
   public:
    // |stream| and |encapsulated_client_session| must be valid for the lifetime
    // of the ConnectUdpClientState.
    explicit ConnectUdpClientState(
        QuicSpdyClientStream* stream,
        EncapsulatedClientSession* encapsulated_client_session,
        MasqueClientSession* masque_session,
        absl::optional<QuicDatagramContextId> context_id,
        const QuicSocketAddress& target_server_address);

    ~ConnectUdpClientState();

    // Disallow copy but allow move.
    ConnectUdpClientState(const ConnectUdpClientState&) = delete;
    ConnectUdpClientState(ConnectUdpClientState&&);
    ConnectUdpClientState& operator=(const ConnectUdpClientState&) = delete;
    ConnectUdpClientState& operator=(ConnectUdpClientState&&);

    QuicSpdyClientStream* stream() const { return stream_; }
    EncapsulatedClientSession* encapsulated_client_session() const {
      return encapsulated_client_session_;
    }
    absl::optional<QuicDatagramContextId> context_id() const {
      return context_id_;
    }
    const QuicSocketAddress& target_server_address() const {
      return target_server_address_;
    }

    // From QuicSpdyStream::Http3DatagramVisitor.
    void OnHttp3Datagram(QuicStreamId stream_id,
                         absl::optional<QuicDatagramContextId> context_id,
                         absl::string_view payload) override;

    // From QuicSpdyStream::Http3DatagramRegistrationVisitor.
    void OnContextReceived(QuicStreamId stream_id,
                           absl::optional<QuicDatagramContextId> context_id,
                           DatagramFormatType format_type,
                           absl::string_view format_additional_data) override;
    void OnContextClosed(QuicStreamId stream_id,
                         absl::optional<QuicDatagramContextId> context_id,
                         ContextCloseCode close_code,
                         absl::string_view close_details) override;

   private:
    QuicSpdyClientStream* stream_;                            // Unowned.
    EncapsulatedClientSession* encapsulated_client_session_;  // Unowned.
    MasqueClientSession* masque_session_;                     // Unowned.
    absl::optional<QuicDatagramContextId> context_id_;
    QuicSocketAddress target_server_address_;
  };

  HttpDatagramSupport LocalHttpDatagramSupport() override {
    return HttpDatagramSupport::kDraft00And04;
  }

  const ConnectUdpClientState* GetOrCreateConnectUdpClientState(
      const QuicSocketAddress& target_server_address,
      EncapsulatedClientSession* encapsulated_client_session);

  MasqueMode masque_mode_;
  std::string uri_template_;
  std::list<ConnectUdpClientState> connect_udp_client_states_;
  absl::flat_hash_map<QuicConnectionId,
                      EncapsulatedClientSession*,
                      QuicConnectionIdHash>
      client_connection_id_registrations_;
  Owner* owner_;  // Unowned;
  MasqueCompressionEngine compression_engine_;
};

}  // namespace quic

#endif  // QUICHE_QUIC_MASQUE_MASQUE_CLIENT_SESSION_H_
