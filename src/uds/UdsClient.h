#pragma once

#include "uds/UdsTypes.h"
#include <functional>
#include <memory>
#include <vector>

class DoipClient;

class UdsClient {
public:
  using UdsResponseCallback = std::function<void(const UdsResponse&)>;

  explicit UdsClient(std::shared_ptr<DoipClient> doip);
  ~UdsClient() = default;

  void DiagnosticSessionControl(DiagnosticSession session,
                                UdsResponseCallback cb = nullptr);
  void EcuReset(EcuResetType type, UdsResponseCallback cb = nullptr);
  void TesterPresent(UdsResponseCallback cb = nullptr);

  void ReadDtcByStatusMask(uint8_t status_mask,
                           DtcFormatType format = DtcFormatType::StatusGroup,
                           UdsResponseCallback cb = nullptr);
  void ReadDtcSnapshot(uint32_t dtc_number,
                       UdsResponseCallback cb = nullptr);
  void ClearDiagnosticInformation(uint16_t group = 0xFFFF,
                                  UdsResponseCallback cb = nullptr);

  void ReadDataByIdentifier(uint16_t did,
                            UdsResponseCallback cb = nullptr);
  void WriteDataByIdentifier(uint16_t did,
                             const std::vector<uint8_t>& data,
                             UdsResponseCallback cb = nullptr);

  void SecurityAccess(uint8_t subfunction,
                      const std::vector<uint8_t>& data,
                      UdsResponseCallback cb = nullptr);

  DiagnosticSession GetCurrentSession() const;

  void SetRequestTimeoutMs(int timeout_ms);
  void SetDefaultCallback(UdsResponseCallback cb);

  static UdsResponse ParseResponse(const std::vector<uint8_t>& raw);

private:
  void SendRequest(const std::vector<uint8_t>& request,
                   UdsResponseCallback cb);
  static std::string NrcToString(uint8_t nrc);

  std::shared_ptr<DoipClient> doip_;
  DiagnosticSession current_session_{DiagnosticSession::Default};
  int timeout_ms_{5000};
  UdsResponseCallback default_cb_;
};
