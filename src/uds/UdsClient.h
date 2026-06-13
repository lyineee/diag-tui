#pragma once

#include "uds/UdsMessage.h"
#include <functional>
#include <memory>
#include <vector>

class DoipClient;

class UdsClient {
public:
  using UdsResponseCallback = std::function<void(const DiagResponse&)>;

  explicit UdsClient(std::shared_ptr<DoipClient> doip);
  ~UdsClient() = default;

  void DiagnosticSessionControl(uint8_t session,
                                UdsResponseCallback cb = nullptr);
  void EcuReset(uint8_t type, UdsResponseCallback cb = nullptr);
  void TesterPresent(UdsResponseCallback cb = nullptr);

  void ReadDtcByStatusMask(uint8_t status_mask,
                           UdsResponseCallback cb = nullptr);
  void ReportNumberOfDTCByStatusMask(uint8_t status_mask,
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

  uint8_t GetCurrentSession() const;
  void SetDefaultCallback(UdsResponseCallback cb);

private:
  void SendRequest(const DiagnosticRequest& req,
                   UdsResponseCallback cb);

  std::shared_ptr<DoipClient> doip_;
  uint8_t current_session_{0x01};
  UdsResponseCallback default_cb_;
};
