// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef QUICHE_QUIC_QBONE_QBONE_SERVER_SESSION_H_
#define QUICHE_QUIC_QBONE_QBONE_SERVER_SESSION_H_

#include "absl/strings/string_view.h"
#include "quic/core/quic_crypto_server_stream_base.h"
#include "quic/core/quic_crypto_stream.h"
#include "quic/platform/api/quic_export.h"
#include "quic/qbone/qbone_control.pb.h"
#include "quic/qbone/qbone_control_stream.h"
#include "quic/qbone/qbone_packet_processor.h"
#include "quic/qbone/qbone_packet_writer.h"
#include "quic/qbone/qbone_session_base.h"

namespace quic {

// A helper class is used by the QuicCryptoServerStream.
class QboneCryptoServerStreamHelper
    : public QuicCryptoServerStreamBase::Helper {
 public:
  // This will look for the QBONE alpn.
  bool CanAcceptClientHello(const CryptoHandshakeMessage& chlo,
                            const QuicSocketAddress& client_address,
                            const QuicSocketAddress& peer_address,
                            const QuicSocketAddress& self_address,
                            std::string* error_details) const override;
};

class QUIC_EXPORT_PRIVATE QboneServerSession
    : public QboneSessionBase,
      public QbonePacketProcessor::OutputInterface,
      public QbonePacketProcessor::StatsInterface {
 public:
  QboneServerSession(const quic::ParsedQuicVersionVector& supported_versions,
                     QuicConnection* connection,
                     Visitor* owner,
                     const QuicConfig& config,
                     const QuicCryptoServerConfig* quic_crypto_server_config,
                     QuicCompressedCertsCache* compressed_certs_cache,
                     QbonePacketWriter* writer,
                     QuicIpAddress self_ip,
                     QuicIpAddress client_ip,
                     size_t client_ip_subnet_length,
                     QboneServerControlStream::Handler* handler);
  QboneServerSession(const QboneServerSession&) = delete;
  QboneServerSession& operator=(const QboneServerSession&) = delete;
  ~QboneServerSession() override;

  void Initialize() override;
  // Override to create control stream at FORWARD_SECURE encryption level.
  void SetDefaultEncryptionLevel(quic::EncryptionLevel level) override;

  virtual bool SendClientRequest(const QboneClientRequest& request);

  void ProcessPacketFromNetwork(absl::string_view packet) override;
  void ProcessPacketFromPeer(absl::string_view packet) override;

  // QbonePacketProcessor::OutputInterface implementation.
  void SendPacketToClient(absl::string_view packet) override;
  void SendPacketToNetwork(absl::string_view packet) override;

  // QbonePacketProcessor::StatsInterface implementation.
  void OnPacketForwarded(QbonePacketProcessor::Direction direction) override {}
  void OnPacketDroppedSilently(
      QbonePacketProcessor::Direction direction) override {}
  void OnPacketDroppedWithIcmp(
      QbonePacketProcessor::Direction direction) override {}
  void OnPacketDroppedWithTcpReset(
      QbonePacketProcessor::Direction direction) override {}
  void OnPacketDeferred(QbonePacketProcessor::Direction direction) override {}

 protected:
  // QboneSessionBase interface implementation.
  std::unique_ptr<QuicCryptoStream> CreateCryptoStream() override;

  // Instantiate QboneServerControlStream.
  void CreateControlStream();

  // The packet processor.
  QbonePacketProcessor processor_;

  // Config for QUIC crypto server stream, used by the server.
  const QuicCryptoServerConfig* quic_crypto_server_config_;

 private:
  // Used by QUIC crypto server stream to track most recently compressed certs.
  QuicCompressedCertsCache* compressed_certs_cache_;
  // This helper is needed when create QuicCryptoServerStream.
  QboneCryptoServerStreamHelper stream_helper_;
  // Passed to the control stream.
  QboneServerControlStream::Handler* handler_;
  // The unowned control stream.
  QboneServerControlStream* control_stream_ = nullptr;
};

}  // namespace quic

#endif  // QUICHE_QUIC_QBONE_QBONE_SERVER_SESSION_H_
