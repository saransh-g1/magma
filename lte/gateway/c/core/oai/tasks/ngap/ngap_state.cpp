/**
 * Copyright 2020 The Magma Authors.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
/****************************************************************************
  Source      ngap_state.cpp
  Date        2020/07/28
  Author      Ashish Prajapati
  Subsystem   Access and Mobility Management Function
  Description Defines Access to states

*****************************************************************************/

#include "lte/gateway/c/core/oai/tasks/ngap/ngap_state.hpp"
#include "lte/gateway/c/core/oai/tasks/ngap/ngap_types.h"
#include <cstdlib>
#include <cstring>

#include <memory.h>

extern "C" {
#include "lte/gateway/c/core/oai/lib/bstr/bstrlib.h"

#include "lte/gateway/c/core/common/assertions.h"
#include "lte/gateway/c/core/common/common_defs.h"
#include "lte/gateway/c/core/common/dynamic_memory_check.h"
}

#include "lte/gateway/c/core/oai/tasks/ngap/ngap_state_manager.hpp"

using magma5g::NgapStateManager;

int ngap_state_init(uint32_t max_ues, uint32_t max_gnbs, bool use_stateless) {
  OAILOG_FUNC_IN(LOG_NGAP);
  NgapStateManager::getInstance().init(max_ues, max_gnbs, use_stateless);
  OAILOG_FUNC_RETURN(LOG_NGAP, RETURNok);
}

ngap_state_t* get_ngap_state(bool read_from_db) {
  return NgapStateManager::getInstance().get_state(read_from_db);
}

void ngap_state_exit() { NgapStateManager::getInstance().free_state(); }

void put_ngap_state() { NgapStateManager::getInstance().write_state_to_db(); }

gnb_description_t* ngap_state_get_gnb(ngap_state_t* state,
                                      sctp_assoc_id_t assoc_id) {
  OAILOG_FUNC_IN(LOG_NGAP);
  gnb_description_t* gnb = nullptr;

  hashtable_ts_get(&state->gnbs, (const hash_key_t)assoc_id, reinterpret_cast<void**>(&gnb));
  OAILOG_FUNC_RETURN(LOG_NGAP, gnb);
}

m5g_ue_description_t* ngap_state_get_ue_gnbid(sctp_assoc_id_t sctp_assoc_id,
                                              gnb_ue_ngap_id_t gnb_ue_ngap_id) {
  OAILOG_FUNC_IN(LOG_NGAP);
  m5g_ue_description_t* ue = nullptr;

  hash_table_ts_t* state_ue_ht = get_ngap_ue_state();
  uint64_t comp_ngap_id = static_cast<uint64_t>(gnb_ue_ngap_id) << 32 | sctp_assoc_id;
  hashtable_ts_get(state_ue_ht, (const hash_key_t)comp_ngap_id, reinterpret_cast<void**>(&ue));

  OAILOG_FUNC_RETURN(LOG_NGAP, ue);
}

m5g_ue_description_t* ngap_state_get_ue_amfid(amf_ue_ngap_id_t amf_ue_ngap_id) {
  OAILOG_FUNC_IN(LOG_NGAP);
  m5g_ue_description_t* ue = nullptr;

  hash_table_ts_t* state_ue_ht = get_ngap_ue_state();
  hashtable_ts_apply_callback_on_elements((hash_table_ts_t* const)state_ue_ht,
                                          ngap_ue_compare_by_amf_ue_id_cb,
                                          &amf_ue_ngap_id, reinterpret_cast<void**>(&ue));

  OAILOG_FUNC_RETURN(LOG_NGAP, ue);
}

m5g_ue_description_t* ngap_state_get_ue_imsi(imsi64_t imsi64) {
  OAILOG_FUNC_IN(LOG_NGAP);
  m5g_ue_description_t* ue = nullptr;

  hash_table_ts_t* state_ue_ht = get_ngap_ue_state();
  hashtable_ts_apply_callback_on_elements((hash_table_ts_t* const)state_ue_ht,
                                          ngap_ue_compare_by_imsi, &imsi64,
                                          reinterpret_cast<void**>(&ue));

  OAILOG_FUNC_RETURN(LOG_NGAP, ue);
}

uint64_t ngap_get_comp_ngap_id(sctp_assoc_id_t sctp_assoc_id,
                               gnb_ue_ngap_id_t gnb_ue_ngap_id) {
  return static_cast<uint64_t>(gnb_ue_ngap_id) << 32 | sctp_assoc_id;;
}

void put_ngap_imsi_map() {
  NgapStateManager::getInstance().put_ngap_imsi_map();
}

ngap_imsi_map_t* get_ngap_imsi_map() {
  return NgapStateManager::getInstance().get_ngap_imsi_map();
}

bool ngap_ue_compare_by_amf_ue_id_cb(__attribute__((unused))
                                     const hash_key_t keyP,
                                     void* const elementP, void* parameterP,
                                     void** resultP) {
  amf_ue_ngap_id_t* amf_ue_ngap_id_p = (amf_ue_ngap_id_t*)parameterP;
  m5g_ue_description_t* ue_ref = (m5g_ue_description_t*)elementP;
  OAILOG_FUNC_IN(LOG_NGAP);
  if (*amf_ue_ngap_id_p == ue_ref->amf_ue_ngap_id) {
    *resultP = elementP;
    OAILOG_TRACE(LOG_NGAP,
                 "Found ue_ref %p amf_ue_ngap_id " MME_UE_NGAP_ID_FMT "\n",
                 ue_ref, ue_ref->amf_ue_ngap_id);
    OAILOG_FUNC_RETURN(LOG_NGAP, true);
  }
  OAILOG_FUNC_RETURN(LOG_NGAP, false);
}

bool ngap_ue_compare_by_imsi(__attribute__((unused)) const hash_key_t keyP,
                             void* const elementP, void* parameterP,
                             void** resultP) {
  imsi64_t imsi64 = INVALID_IMSI64;
  imsi64_t* target_imsi64 = (imsi64_t*)parameterP;
  m5g_ue_description_t* ue_ref = (m5g_ue_description_t*)elementP;

  ngap_imsi_map_t* imsi_map = get_ngap_imsi_map();
  hashtable_uint64_ts_get(imsi_map->amf_ue_id_imsi_htbl,
                          (const hash_key_t)ue_ref->amf_ue_ngap_id, &imsi64);
  OAILOG_FUNC_IN(LOG_NGAP);
  if (*target_imsi64 != INVALID_IMSI64 && *target_imsi64 == imsi64) {
    *resultP = elementP;
    OAILOG_DEBUG_UE(LOG_NGAP, imsi64, "Found ue_ref\n");
    OAILOG_FUNC_RETURN(LOG_NGAP, true);
  }
  OAILOG_FUNC_RETURN(LOG_NGAP, false);
}

hash_table_ts_t* get_ngap_ue_state(void) {
  return NgapStateManager::getInstance().get_ue_state_ht();
}

void put_ngap_ue_state(imsi64_t imsi64) {
  OAILOG_FUNC_IN(LOG_NGAP);
  if (NgapStateManager::getInstance().is_persist_state_enabled()) {
    m5g_ue_description_t* ue_ctxt = ngap_state_get_ue_imsi(imsi64);
    if (ue_ctxt) {
      auto imsi_str = NgapStateManager::getInstance().get_imsi_str(imsi64);
      NgapStateManager::getInstance().write_ue_state_to_db(ue_ctxt, imsi_str);
    }
  }
  OAILOG_FUNC_OUT(LOG_NGAP);
}

void delete_ngap_ue_state(imsi64_t imsi64) {
  OAILOG_FUNC_IN(LOG_NGAP);
  auto imsi_str = NgapStateManager::getInstance().get_imsi_str(imsi64);
  NgapStateManager::getInstance().clear_ue_state_db(imsi_str);
  OAILOG_FUNC_OUT(LOG_NGAP);
}
