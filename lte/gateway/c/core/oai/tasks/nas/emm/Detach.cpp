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

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif
#include "lte/gateway/c/core/common/assertions.h"
#include "lte/gateway/c/core/common/common_defs.h"
#include "lte/gateway/c/core/oai/common/common_types.h"
#include "lte/gateway/c/core/oai/common/log.h"
#include "lte/gateway/c/core/oai/lib/bstr/bstrlib.h"
#ifdef __cplusplus
}
#endif

#include "lte/gateway/c/core/common/dynamic_memory_check.h"
#include "lte/gateway/c/core/oai/include/mme_app_ue_context.hpp"
#include "lte/gateway/c/core/oai/include/mme_app_statistics.hpp"
#include "lte/gateway/c/core/oai/include/mme_events.hpp"
#include "lte/gateway/c/core/oai/lib/3gpp/3gpp_36.401.h"
#include "lte/gateway/c/core/oai/tasks/mme_app/mme_app_defs.hpp"
#include "lte/gateway/c/core/oai/tasks/mme_app/mme_app_timer.hpp"
#include "lte/gateway/c/core/oai/tasks/nas/api/mme/mme_api.hpp"
#include "lte/gateway/c/core/oai/tasks/nas/emm/emm_data.hpp"
#include "lte/gateway/c/core/oai/tasks/nas/emm/emm_proc.hpp"
#include "lte/gateway/c/core/oai/tasks/nas/emm/msg/DetachRequest.hpp"
#include "lte/gateway/c/core/oai/tasks/nas/emm/sap/emm_asDef.hpp"
#include "lte/gateway/c/core/oai/tasks/nas/emm/sap/emm_fsm.hpp"
#include "lte/gateway/c/core/oai/tasks/nas/emm/sap/emm_sap.hpp"
#include "lte/gateway/c/core/oai/tasks/nas/esm/esm_data.hpp"
#include "lte/gateway/c/core/oai/tasks/nas/esm/sap/esm_sap.hpp"
#include "lte/gateway/c/core/oai/tasks/nas/esm/sap/esm_sapDef.hpp"
#include "lte/gateway/c/core/oai/tasks/nas/nas_procedures.hpp"
#include "lte/gateway/c/core/oai/tasks/nas/util/nas_timer.hpp"
#include "orc8r/gateway/c/common/service303/MetricsHelpers.hpp"

/****************************************************************************/
/****************  E X T E R N A L    D E F I N I T I O N S  ****************/
/****************************************************************************/
status_code_e esm_sap_send(esm_sap_t* msg);
/****************************************************************************/
/*******************  L O C A L    D E F I N I T I O N S  *******************/
/****************************************************************************/

/* String representation of the detach type */
static const char* emm_detach_type_str[] = {"EPS",
                                            "IMSI",
                                            "EPS/IMSI",
                                            "RE-ATTACH REQUIRED",
                                            "RE-ATTACH NOT REQUIRED",
                                            "RESERVED"};

/* String representation of the sgs detach type */
static const char* emm_sgs_detach_type_str[] = {"EPS",
                                                "UE-INITIATED-EXPLICIT-NONEPS",
                                                "COMBINED",
                                                "NW-INITIATED-EPS",
                                                "NW-INITIATED-IMPLICIT-NONEPS",
                                                "RESERVED"};
/****************************************************************************
 **                                                                        **
 ** Name:    mme_app_handle_detach_t3422_expiry() **
 **                                                                        **
 ** Description: T3422 timeout handler                                     **
 **      Upon T3422 timer expiration, the Detach request                   **
 **      message is retransmitted and the timer restarted. When            **
 **      retransmission counter is exceed, the MME shall abort the         **
 **      detach procedure and perform implicit detach.                     **
 **                                                                        **
 **                                                                        **
 ** Inputs:  args:      handler parameters                                 **
 **                                                                        **
 **                                                                        **
 ** Outputs:     None                                                      **
 **      Return:    None                                                   **
 **      Others:    None                                                   **
 **                                                                        **
 ***************************************************************************/
status_code_e mme_app_handle_detach_t3422_expiry(zloop_t* loop, int timer_id,
                                                 void* args) {
  OAILOG_FUNC_IN(LOG_NAS_EMM);

  mme_ue_s1ap_id_t mme_ue_s1ap_id = 0;
  nw_detach_data_t* data = NULL;
  emm_context_t* emm_ctx = NULL;
  // if args is not NULL, then the function is called during start up
  // as part of resumed timers.
  if (args) {
    data = (nw_detach_data_t*)(args);
    mme_ue_s1ap_id = data->ue_id;
    emm_ctx = emm_context_get(&_emm_data, mme_ue_s1ap_id);
  } else {
    if (!mme_pop_timer_arg_ue_id(timer_id, &mme_ue_s1ap_id)) {
      OAILOG_WARNING(LOG_NAS_EMM, "Invalid Timer Id expiration, Timer Id: %u\n",
                     timer_id);
      OAILOG_FUNC_RETURN(LOG_NAS_EMM, RETURNok);
    }

    struct ue_mm_context_s* ue_context_p = mme_app_get_ue_context_for_timer(
        mme_ue_s1ap_id, const_cast<char*>("Detach Procedure T3422 Timer"));
    if (ue_context_p == NULL) {
      OAILOG_ERROR(
          LOG_MME_APP,
          "Invalid UE context received, MME UE S1AP Id: " MME_UE_S1AP_ID_FMT
          "\n",
          mme_ue_s1ap_id);
      OAILOG_FUNC_RETURN(LOG_NAS_EMM, RETURNok);
    }

    emm_ctx = &ue_context_p->emm_context;
    data = (nw_detach_data_t*)emm_ctx->t3422_arg;
  }

  if (!data) {
    OAILOG_ERROR_UE(LOG_NAS_EMM, emm_ctx->_imsi64,
                    "The argument for network initiated"
                    "detach timer is NULL \n");
    OAILOG_FUNC_RETURN(LOG_NAS_EMM, RETURNok);
  }

  // Increment the retransmission counter
  data->retransmission_count += 1;
  OAILOG_WARNING_UE(LOG_NAS_EMM, emm_ctx->_imsi64,
                    "EMM-PROC: T3422 timer expired,retransmission "
                    "counter = %d for ue id " MME_UE_S1AP_ID_FMT "\n",
                    data->retransmission_count, mme_ue_s1ap_id);

  if (data->retransmission_count < DETACH_REQ_COUNTER_MAX) {
    // Resend detach request message to the UE
    emm_proc_nw_initiated_detach_request(mme_ue_s1ap_id, data->detach_type);
  } else {
    // Abort the detach procedure and perform implicit detach
    if (data->detach_type != NW_DETACH_TYPE_IMSI_DETACH) {
      emm_detach_request_ies_t emm_detach_request_params;
      /*
       * This is implicit detach procedure, therefore, setting detach type as
       * switched-off to avoid sending of detach accept message
       */
      emm_detach_request_params.switch_off = 1;
      emm_detach_request_params.type = EMM_DETACH_TYPE_EPS;  // 0
      emm_proc_detach_request(mme_ue_s1ap_id, &emm_detach_request_params);
    }
    if (data) {
      // Free timer argument
      free_wrapper(reinterpret_cast<void**>(&data));
      emm_ctx->t3422_arg = NULL;
    }
  }
  OAILOG_FUNC_RETURN(LOG_NAS_EMM, RETURNok);
}

status_code_e release_esm_pdn_context(emm_context_t* emm_context,
                                      mme_ue_s1ap_id_t ue_id) {
  OAILOG_FUNC_IN(LOG_NAS_EMM);
  esm_sap_t esm_sap = {};
  esm_sap.primitive = ESM_EPS_BEARER_CONTEXT_DEACTIVATE_REQ;
  esm_sap.ue_id = ue_id;
  esm_sap.ctx = emm_context;
  esm_sap.data.eps_bearer_context_deactivate.ebi[0] = ESM_SAP_ALL_EBI;
  esm_sap.data.eps_bearer_context_deactivate.is_pcrf_initiated = false;

  status_code_e rc = esm_sap_send(&esm_sap);
  OAILOG_FUNC_RETURN(LOG_NAS_EMM, rc);
}

void clear_emm_ctxt(emm_context_t* emm_context) {
  OAILOG_FUNC_IN(LOG_NAS_EMM);
  if (!emm_context) {
    return;
  }
  mme_ue_s1ap_id_t ue_id =
      PARENT_STRUCT(emm_context, struct ue_mm_context_s, emm_context)
          ->mme_ue_s1ap_id;

  nas_delete_all_emm_procedures(emm_context);
  // Stop T3489 timer
  free_esm_context_content(&emm_context->esm_ctx);

  // Change the FSM state to Deregistered
  if (emm_fsm_get_state(emm_context) != EMM_DEREGISTERED) {
    emm_fsm_set_state(ue_id, emm_context, EMM_DEREGISTERED);
  }

  emm_ctx_clear_old_guti(emm_context);
  emm_ctx_clear_guti(emm_context);
  emm_ctx_clear_imsi(emm_context);
  emm_ctx_clear_imei(emm_context);
  emm_ctx_clear_auth_vectors(emm_context);
  emm_ctx_clear_security(emm_context);
  emm_ctx_clear_non_current_security(emm_context);
  OAILOG_FUNC_OUT(LOG_NAS_EMM);
}

/*
   --------------------------------------------------------------------------
        Internal data handled by the detach procedure in the UE
   --------------------------------------------------------------------------
*/

/*
   --------------------------------------------------------------------------
        Internal data handled by the detach procedure in the MME
   --------------------------------------------------------------------------
*/

/****************************************************************************/
/******************  E X P O R T E D    F U N C T I O N S  ******************/
/****************************************************************************/

/*
   --------------------------------------------------------------------------
            Detach procedure executed by the UE
   --------------------------------------------------------------------------
*/

/****************************************************************************
 **                                                                        **
 ** Name:    emm_proc_sgs_detach_request                                   **
 **                                                                        **
 ** Description: Performs the UE/NW initiated SGS detach procedure for EPS **
 **              and non EPS services                                      **
 ** Inputs:  ue_id:      UE lower layer identifier                         **
 **      type:      Type of the SGS detach                                 **
 **      Others:    _emm_data                                              **
 **                                                                        **
 ** Outputs:     None                                                      **
 **      Return:    RETURNok, RETURNerror                                  **
 **      Others:    None                                                   **
 **                                                                        **
 ***************************************************************************/
status_code_e emm_proc_sgs_detach_request(
    mme_ue_s1ap_id_t ue_id, emm_proc_sgs_detach_type_t sgs_detach_type) {
  OAILOG_FUNC_IN(LOG_NAS_EMM);

  OAILOG_INFO(LOG_NAS_EMM,
              "EMM-PROC  - SGS Detach type = %s (%d) requested "
              "(ue_id=" MME_UE_S1AP_ID_FMT ") \n",
              emm_sgs_detach_type_str[sgs_detach_type], sgs_detach_type, ue_id);
  /*
   * Get the UE emm context
   */

  emm_context_t* emm_ctx = emm_context_get(&_emm_data, ue_id);

  if (emm_ctx != NULL) {
    /* check if the non EPS service control is enable and combined attach*/
    if (((_esm_data.conf.features & MME_API_SMS_SUPPORTED) ||
         (_esm_data.conf.features & MME_API_CSFB_SMS_SUPPORTED)) &&
        (emm_ctx->attach_type == EMM_ATTACH_TYPE_COMBINED_EPS_IMSI)) {
      // Notify MME APP to trigger SGS Detach Indication  towards SGS task.
      mme_app_handle_sgs_detach_req(
          (PARENT_STRUCT(emm_ctx, struct ue_mm_context_s, emm_context)),
          sgs_detach_type);
    }
  }
  OAILOG_FUNC_RETURN(LOG_NAS_EMM, RETURNok);
}
/****************************************************************************
 **                                                                        **
 ** Name:    emm_proc_detach_request()                                 **
 **                                                                        **
 ** Description: Performs the UE initiated detach procedure for EPS servi- **
 **      ces only When the DETACH REQUEST message is received by   **
 **      the network.                                              **
 **                                                                        **
 **              3GPP TS 24.301, section 5.5.2.2.2                         **
 **      Upon receiving the DETACH REQUEST message the network     **
 **      shall send a DETACH ACCEPT message to the UE and store    **
 **      the current EPS security context, if the detach type IE   **
 **      does not indicate "switch off". Otherwise, the procedure  **
 **      is completed when the network receives the DETACH REQUEST **
 **      message.                                                  **
 **      The network shall deactivate the EPS bearer context(s)    **
 **      for this UE locally without peer-to-peer signalling and   **
 **      shall enter state EMM-DEREGISTERED.                       **
 **                                                                        **
 ** Inputs:  ue_id:      UE lower layer identifier                  **
 **      type:      Type of the requested detach               **
 **      switch_off:    Indicates whether the detach is required   **
 **             because the UE is switched off or not      **
 **      native_ksi:    true if the security context is of type    **
 **             native                                     **
 **      ksi:       The NAS ket sey identifier                 **
 **      guti:      The GUTI if provided by the UE             **
 **      imsi:      The IMSI if provided by the UE             **
 **      imei:      The IMEI if provided by the UE             **
 **      Others:    _emm_data                                  **
 **                                                                        **
 ** Outputs:     None                                                      **
 **      Return:    RETURNok, RETURNerror                      **
 **      Others:    None                                       **
 **                                                                        **
 ***************************************************************************/
status_code_e emm_proc_detach_request(mme_ue_s1ap_id_t ue_id,
                                      emm_detach_request_ies_t* params)

{
  OAILOG_FUNC_IN(LOG_NAS_EMM);
  status_code_e rc = RETURNok;

  OAILOG_INFO(LOG_NAS_EMM,
              "EMM-PROC  - Detach type = %s (%d) requested"
              " (ue_id=" MME_UE_S1AP_ID_FMT ")\n",
              emm_detach_type_str[params->type], params->type, ue_id);
  /*
   * Get the UE context and emm context
   */
  ue_mm_context_t* ue_context_p = NULL;
  ue_context_p = mme_ue_context_exists_mme_ue_s1ap_id(ue_id);
  emm_context_t* emm_ctx = NULL;
  if (ue_context_p) {
    emm_ctx = &ue_context_p->emm_context;
  }

  if (emm_ctx == NULL) {
    OAILOG_WARNING(LOG_NAS_EMM,
                   "No EMM context exists for the UE (ue_id=" MME_UE_S1AP_ID_FMT
                   ") \n",
                   ue_id);
    increment_counter("ue_detach", 1, 2, "result", "failure", "cause",
                      "no_emm_context");
    // There may be MME APP Context. Trigger clean up in MME APP
    mme_app_handle_detach_req(ue_id);
    OAILOG_FUNC_RETURN(LOG_NAS_EMM, RETURNok);
  }

  if (params->switch_off) {
    increment_counter("ue_detach", 1, 1, "result", "success");
    increment_counter("ue_detach", 1, 1, "action", "detach_accept_not_sent");
    detach_success_event(emm_ctx->_imsi64, "detach_accept_not_sent");
    if (ue_context_p->ecm_state == ECM_CONNECTED) {
      update_mme_app_stats_connected_ue_sub();
    }
    rc = RETURNok;
  } else {
    /*
     * Normal detach without UE switch-off
     */
    emm_sap_t emm_sap = {};
    emm_as_data_t* emm_as = &emm_sap.u.emm_as.u.data;

    /*
     * Setup NAS information message to transfer
     */
    emm_as->nas_info = EMM_AS_NAS_DATA_DETACH_ACCEPT;
    emm_as->nas_msg = NULL;
    /*
     * Set the UE identifier
     */
    emm_as->ue_id = ue_id;
    /*
     * Setup EPS NAS security data
     */
    emm_as_set_security_data(&emm_as->sctx, &emm_ctx->_security, false, true);
    /*
     * Notify EMM-AS SAP that Detach Accept message has to
     * be sent to the network
     */
    emm_sap.primitive = EMMAS_DATA_REQ;
    rc = emm_sap_send(&emm_sap);
    increment_counter("ue_detach", 1, 1, "result", "success");
    increment_counter("ue_detach", 1, 1, "action", "detach_accept_sent");
    detach_success_event(emm_ctx->_imsi64, "detach_accept_sent");
    /*
     * If Detach request is recieved for IMSI only then don't trigger session
     * release and don't clear emm context return from here
     */
    if (params->type == EMM_DETACH_TYPE_IMSI) {
      OAILOG_INFO_UE(
          LOG_NAS_EMM, emm_ctx->_imsi64,
          "Do not clear emm context for UE Initiated IMSI Detach Request "
          " for the UE (ue_id=" MME_UE_S1AP_ID_FMT ")\n",
          ue_id);
      OAILOG_FUNC_RETURN(LOG_NAS_EMM, RETURNok);
    }
  }
  if (rc != RETURNerror) {
    emm_sap_t emm_sap = {};

    /*
     * Notify EMM FSM that the UE has been implicitly detached
     */
    emm_sap.primitive = EMMREG_DETACH_REQ;
    emm_sap.u.emm_reg.ue_id = ue_id;
    emm_sap.u.emm_reg.ctx = emm_ctx;
    rc = emm_sap_send(&emm_sap);
    /* Notify MME APP to trigger Session release towards SGW and S1 signaling
     * release towards S1AP.
     */
    mme_app_handle_detach_req(ue_id);
  }
  OAILOG_FUNC_RETURN(LOG_NAS_EMM, RETURNok);
}

/****************************************************************************
 **                                                                        **
 ** Name:    emm_proc_detach_accept()                                      **
 **                                                                        **
 ** Description: Trigger clean up of UE context in ESM/EMM,MME_APP,SGPW    **
 **      and S1AP                                                          **
 **                                                                        **
 ***************************************************************************/
status_code_e emm_proc_detach_accept(mme_ue_s1ap_id_t ue_id) {
  OAILOG_FUNC_IN(LOG_NAS_EMM);
  emm_context_t* emm_ctx = NULL;

  /*
   * Get the UE context
   */
  emm_ctx = emm_context_get(&_emm_data, ue_id);

  if (emm_ctx == NULL) {
    OAILOG_WARNING(LOG_NAS_EMM,
                   "No EMM context exists for the UE (ue_id=" MME_UE_S1AP_ID_FMT
                   ")",
                   ue_id);
    // There may be MME APP Context. Trigger clean up in MME APP
    mme_app_handle_detach_req(ue_id);
    OAILOG_FUNC_RETURN(LOG_NAS_EMM, RETURNok);
  }

  // Stop T3422
  if (emm_ctx->T3422.id != NAS_TIMER_INACTIVE_ID) {
    OAILOG_DEBUG_UE(LOG_NAS_EMM, emm_ctx->_imsi64,
                    "EMM-PROC  - Stop timer T3422 (%ld) for ue_id %d \n",
                    emm_ctx->T3422.id, ue_id);
    nas_stop_T3422(emm_ctx->_imsi64, &(emm_ctx->T3422));
    if (emm_ctx->t3422_arg) {
      free_wrapper(&emm_ctx->t3422_arg);
      emm_ctx->t3422_arg = NULL;
    }
  }

  // if detach type = IMSI_DETACH, we are not clearing the UE context
  if (emm_ctx->is_imsi_only_detach == false) {
    emm_sap_t emm_sap = {};
    /*
     * Notify EMM FSM that the UE has been detached
     */
    emm_sap.primitive = EMMREG_DETACH_REQ;
    emm_sap.u.emm_reg.ue_id = ue_id;
    emm_sap.u.emm_reg.ctx = emm_ctx;
    emm_sap_send(&emm_sap);
    // Notify MME APP to trigger Session release towards SGW and S1 signaling
    // release towards S1AP.
    mme_app_handle_detach_req(ue_id);
  } else {
    emm_ctx->is_imsi_only_detach = false;
  }
  OAILOG_FUNC_RETURN(LOG_NAS_EMM, RETURNok);
}

/****************************************************************************
 **                                                                        **
 ** Name:    emm_proc_nw_initiated_detach_request()                        **
 **                                                                        **
 ** Description: Performs NW initiated detach procedure by sending         **
 **      DETACH REQUEST message to UE                                      **
 **                                                                        **
 ***************************************************************************/
status_code_e emm_proc_nw_initiated_detach_request(mme_ue_s1ap_id_t ue_id,
                                                   uint8_t detach_type) {
  OAILOG_FUNC_IN(LOG_NAS_EMM);
  status_code_e rc = RETURNok;
  emm_context_t* emm_ctx = NULL;

  OAILOG_INFO(LOG_NAS_EMM,
              "EMM-PROC  - NW Initiated Detach Requested for the UE "
              "(ue_id=" MME_UE_S1AP_ID_FMT ")",
              ue_id);
  /*
   * Get the UE context
   */
  emm_ctx = emm_context_get(&_emm_data, ue_id);

  if (!emm_ctx) {
    OAILOG_WARNING(LOG_NAS_EMM,
                   "No EMM context exists for the UE (ue_id=" MME_UE_S1AP_ID_FMT
                   ")",
                   ue_id);
    OAILOG_FUNC_RETURN(LOG_NAS_EMM, RETURNerror);
  }

  /*
   * Send Detach Request to UE
   */
  emm_sap_t emm_sap = {};
  emm_as_data_t* emm_as = &emm_sap.u.emm_as.u.data;

  /*
   * Setup NAS information message to transfer
   */
  emm_as->nas_info = EMM_AS_NAS_DATA_DETACH_REQ;
  emm_as->nas_msg = NULL;
  emm_as->guti = NULL;
  emm_as->type = detach_type;
  /*
   * Set the UE identifier
   */
  emm_as->ue_id = ue_id;
  /*
   * Setup EPS NAS security data
   */
  emm_as_set_security_data(&emm_as->sctx, &emm_ctx->_security, false, true);
  /*
   * Notify EMM-AS SAP that Detach Request message has to
   * be sent to the network
   */
  emm_sap.primitive = EMMAS_DATA_REQ;
  rc = emm_sap_send(&emm_sap);
  if (rc != RETURNerror) {
    if (emm_ctx->T3422.id != NAS_TIMER_INACTIVE_ID) {
      /*
       * Re-start T3422 timer
       */
      nas_stop_T3422(emm_ctx->_imsi64, &(emm_ctx->T3422));
      nas_start_T3422(ue_id, &(emm_ctx->T3422),
                      (time_out_t)mme_app_handle_detach_t3422_expiry);
    } else {
      /*
       * Start T3422 timer
       */
      if (emm_ctx->t3422_arg) {
        nas_start_T3422(ue_id, &(emm_ctx->T3422),
                        (time_out_t)mme_app_handle_detach_t3422_expiry);
      } else {
        nw_detach_data_t* data = reinterpret_cast<nw_detach_data_t*>(
            calloc(1, sizeof(nw_detach_data_t)));
        if (!data) {
          OAILOG_ERROR_UE(
              LOG_NAS_EMM, emm_ctx->_imsi64,
              "Failed to allocate memory for 3422 timer argument. Didn't start "
              "the 3422 timer for ue id " MME_UE_S1AP_ID_FMT "\n",
              ue_id);
          OAILOG_FUNC_RETURN(LOG_NAS_EMM, RETURNerror);
        }
        data->ue_id = ue_id;
        data->retransmission_count = 0;
        data->detach_type = detach_type;
        emm_ctx->t3422_arg = (void*)data;
        nas_start_T3422(ue_id, &(emm_ctx->T3422),
                        (time_out_t)mme_app_handle_detach_t3422_expiry);
      }
    }
  }
  OAILOG_FUNC_RETURN(LOG_NAS_EMM, RETURNok);
}
//------------------------------------------------------------------------------
void free_emm_detach_request_ies(emm_detach_request_ies_t** const ies) {
  if ((*ies)->guti) {
    free_wrapper(reinterpret_cast<void**>(&(*ies)->guti));
  }
  if ((*ies)->imsi) {
    free_wrapper(reinterpret_cast<void**>(&(*ies)->imsi));
  }
  if ((*ies)->imei) {
    free_wrapper(reinterpret_cast<void**>(&(*ies)->imei));
  }
  free_wrapper(reinterpret_cast<void**>(ies));
}

/****************************************************************************/
/*********************  L O C A L    F U N C T I O N S  *********************/
/****************************************************************************/
