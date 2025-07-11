/*
 * Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The OpenAirInterface Software Alliance licenses this file to You under
 * the terms found in the LICENSE file in the root of this source tree.
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *-------------------------------------------------------------------------------
 * For more information about the OpenAirInterface (OAI) Software Alliance:
 *      contact@openairinterface.org
 */

#include <grpcpp/impl/codegen/status.h>
#include <stdint.h>
#include <string.h>
#include <string>
#include <iostream>
#include "lte/gateway/c/core/oai/common/conversions.h"

#include "lte/gateway/c/core/oai/lib/s6a_proxy/s6a_client_api.hpp"
#include "lte/gateway/c/core/oai/lib/s6a_proxy/S6aClient.hpp"
#include "lte/gateway/c/core/oai/lib/s6a_proxy/proto_msg_to_itti_msg.hpp"
#include "lte/gateway/c/core/oai/common/common_types.h"
#include "feg/protos/s6a_proxy.pb.h"
#include "lte/gateway/c/core/oai/lib/itti/intertask_interface.h"
#include "lte/gateway/c/core/oai/lib/itti/intertask_interface_types.h"
#include "lte/gateway/c/core/oai/lib/itti/itti_types.h"
#include "lte/gateway/c/core/oai/lib/store/sqlite.hpp"
#include "lte/protos/subscriberdb.pb.h"

extern "C" {}

extern task_zmq_ctx_t s6a_task_zmq_ctx;

using namespace magma;
using namespace magma::feg;

bool s6a_purge_ue(const char* imsi) {
  if (imsi == nullptr) {
    return false;
  }
  if (!magma::S6aClient::get_s6a_relay_enabled()) {
    return true;
  }
  magma::S6aClient::purge_ue(imsi, [imsiStr = std::string(imsi)](
                                       grpc::Status status,
                                       feg::PurgeUEAnswer response) {
    auto log_level = "ERROR";
    if (status.ok() &&
        response.error_code() < feg::ErrorCode::COMMAND_UNSUPPORTED) {
      log_level = "INFO";
    }
    // For now, do nothing, just log
    std::cout << "[" << log_level << "] PurgeUE Response for IMSI: " << imsiStr
              << "; Status: " << status.error_message()
              << "; ErrorCode: " << response.error_code() << std::endl;
    return;
  });
  return true;
}

static void s6a_handle_authentication_info_ans(
    const std::string& imsi, uint8_t imsi_length, const grpc::Status& status,
    feg::AuthenticationInformationAnswer response) {
  MessageDef* message_p = NULL;
  s6a_auth_info_ans_t* itti_msg = NULL;

  message_p = itti_alloc_new_message(TASK_S6A, S6A_AUTH_INFO_ANS);
  itti_msg = &message_p->ittiMsg.s6a_auth_info_ans;
  strncpy(itti_msg->imsi, imsi.c_str(), imsi_length);
  itti_msg->imsi_length = imsi_length;

  if (status.ok()) {
    if (response.error_code() < feg::ErrorCode::COMMAND_UNSUPPORTED) {
      std::cout << "[INFO] "
                << "Received S6A-AUTHENTICATION_INFORMATION_ANSWER for IMSI: "
                << imsi << "; Status: " << status.error_message()
                << "; StatusCode: " << response.error_code() << std::endl;

      itti_msg->result.present = S6A_RESULT_BASE;
      itti_msg->result.choice.base = DIAMETER_SUCCESS;
      magma::convert_proto_msg_to_itti_s6a_auth_info_ans(response, itti_msg);
    } else {
      itti_msg->result.present = S6A_RESULT_EXPERIMENTAL;
      itti_msg->result.choice.experimental =
          (s6a_experimental_result_t)response.error_code();
    }
  } else {
    std::cout << "[ERROR] " << status.error_code() << ": "
              << status.error_message() << std::endl;
    std::cout
        << "[ERROR] Received S6A-AUTHENTICATION_INFORMATION_ANSWER for IMSI: "
        << imsi << "; Status: " << status.error_message()
        << "; ErrorCode: " << response.error_code() << std::endl;
    itti_msg->result.present = S6A_RESULT_BASE;
    itti_msg->result.choice.base = DIAMETER_UNABLE_TO_COMPLY;
  }

  IMSI_STRING_TO_IMSI64(reinterpret_cast<char*>(imsi.c_str()), &message_p->ittiMsgHeader.imsi);

  send_msg_to_task(&s6a_task_zmq_ctx, TASK_MME_APP, message_p);
  return;
}

bool s6a_authentication_info_req(const s6a_auth_info_req_t* const air_p) {
  auto imsi_len = air_p->imsi_length;
  std::cout
      << "[INFO] Sending S6A-AUTHENTICATION_INFORMATION_REQUEST with IMSI: "
      << std::string(air_p->imsi) << std::endl;

  magma::S6aClient::authentication_info_req(
      air_p,
      [imsiStr = std::string(air_p->imsi), imsi_len](
          grpc::Status status, feg::AuthenticationInformationAnswer response) {
        s6a_handle_authentication_info_ans(imsiStr, imsi_len, status, response);
      });
  return true;
}

static void s6a_handle_update_location_ans(const std::string& imsi,
                                           uint8_t imsi_length,
                                           uint32_t rat_type,
                                           const grpc::Status& status,
                                           feg::UpdateLocationAnswer response) {
  MessageDef* message_p = NULL;
  s6a_update_location_ans_t* itti_msg = NULL;

  message_p = itti_alloc_new_message(TASK_S6A, S6A_UPDATE_LOCATION_ANS);
  itti_msg = &message_p->ittiMsg.s6a_update_location_ans;

  strncpy(itti_msg->imsi, imsi.c_str(), imsi_length);
  itti_msg->imsi_length = imsi_length;

  if (status.ok()) {
    if (response.error_code() < feg::ErrorCode::COMMAND_UNSUPPORTED) {
      std::cout << "[INFO] Received S6A-LOCATION-UPDATE_ANSWER for IMSI: "
                << imsi << "; Status: " << status.error_message()
                << "; StatusCode: " << response.error_code() << std::endl;

      itti_msg->result.present = S6A_RESULT_BASE;
      itti_msg->result.choice.base = DIAMETER_SUCCESS;
      magma::convert_proto_msg_to_itti_s6a_update_location_ans(response,
                                                               itti_msg);

      // convert ULA response to SubscriberData and write to subscriberdb
      if (S6aClient::get_cloud_subscriberdb_enabled()) {
        magma::lte::SubscriberData sub_data = magma::lte::SubscriberData();
        auto sub_id = sub_data.mutable_sid();
        sub_id->set_id(imsi);
        sub_id->set_type(magma::lte::SubscriberID::IMSI);
        magma::S6aClient::convert_ula_to_subscriber_data(response, &sub_data);
        magma::lte::SqliteStore* sqlObj = new magma::lte::SqliteStore(
            "/var/opt/magma/", 2);  // location is same as SubscriberDB
        sqlObj->add_subscriber(sub_data);
      }

    } else {
      itti_msg->result.present = S6A_RESULT_EXPERIMENTAL;
      itti_msg->result.choice.experimental =
          (s6a_experimental_result_t)response.error_code();
    }
  } else {
    std::cout << "[ERROR] " << status.error_code() << ": "
              << status.error_message() << std::endl;
    std::cout << "[ERROR]  Received S6A-LOCATION-UPDATE_ANSWER for IMSI: "
              << imsi << "; Status: " << status.error_message()
              << "; ErrorCode: " << response.error_code() << std::endl;

    itti_msg->result.present = S6A_RESULT_BASE;
    itti_msg->result.choice.base = DIAMETER_UNABLE_TO_COMPLY;
  }
  std::cout << "[INFO] sent itti S6A-LOCATION-UPDATE_ANSWER for IMSI: " << imsi
            << std::endl;
  IMSI_STRING_TO_IMSI64(reinterpret_cast<char*>(imsi.c_str()), &message_p->ittiMsgHeader.imsi);

  if (rat_type == RAT_NG_RAN) {
    send_msg_to_task(&s6a_task_zmq_ctx, TASK_AMF_APP, message_p);
  } else {
    send_msg_to_task(&s6a_task_zmq_ctx, TASK_MME_APP, message_p);
  }

  return;
}

bool s6a_update_location_req(const s6a_update_location_req_t* const ulr_p) {
  auto imsi_len = ulr_p->imsi_length;
  std::cout << "[DEBUG] Sending S6A-UPDATE_LOCATION_REQUEST with IMSI: "
            << std::string(ulr_p->imsi) << std::endl;

  magma::S6aClient::update_location_request(
      ulr_p, [imsiStr = std::string(ulr_p->imsi), imsi_len,
              rat_type = ulr_p->rat_type](grpc::Status status,
                                          feg::UpdateLocationAnswer response) {
        s6a_handle_update_location_ans(imsiStr, imsi_len, rat_type, status,
                                       response);
      });
  return true;
}
