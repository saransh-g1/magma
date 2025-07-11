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
#include "lte/gateway/c/core/oai/common/common_ies.h"
#include "lte/gateway/c/core/oai/common/common_types.h"
#include "lte/gateway/c/core/oai/common/conversions.h"
#include "lte/gateway/c/core/oai/common/log.h"
#ifdef __cplusplus
}
#endif

#include "lte/gateway/c/core/common/dynamic_memory_check.h"
#include "lte/gateway/c/core/oai/common/common_utility_funs.hpp"
#include "lte/gateway/c/core/oai/include/3gpp_requirements_24.301.h"
#include "lte/gateway/c/core/oai/include/mme_app_state.hpp"
#include "lte/gateway/c/core/oai/include/mme_app_ue_context.hpp"
#include "lte/gateway/c/core/oai/include/mme_config.hpp"
#include "lte/gateway/c/core/oai/lib/3gpp/3gpp_23.003.h"
#include "lte/gateway/c/core/oai/lib/3gpp/3gpp_24.008.h"
#include "lte/gateway/c/core/oai/lib/3gpp/3gpp_24.301.h"
#include "lte/gateway/c/core/oai/lib/3gpp/3gpp_36.401.h"
#include "lte/gateway/c/core/oai/tasks/mme_app/mme_app_defs.hpp"
#include "lte/gateway/c/core/oai/tasks/mme_app/mme_app_itti_messaging.hpp"
#include "lte/gateway/c/core/oai/tasks/nas/api/mme/mme_api.hpp"
#include "lte/gateway/c/core/oai/tasks/nas/emm/EmmCommon.hpp"
#include "lte/gateway/c/core/oai/tasks/nas/emm/emm_data.hpp"
#include "lte/gateway/c/core/oai/tasks/nas/emm/emm_proc.hpp"
#include "lte/gateway/c/core/oai/tasks/nas/emm/msg/emm_cause.hpp"
#include "lte/gateway/c/core/oai/tasks/nas/emm/sap/emm_asDef.hpp"
#include "lte/gateway/c/core/oai/tasks/nas/emm/sap/emm_sap.hpp"
#include "lte/gateway/c/core/oai/tasks/nas/esm/esm_data.hpp"
#include "lte/gateway/c/core/oai/tasks/nas/ies/AdditionalUpdateType.hpp"
#include "lte/gateway/c/core/oai/tasks/nas/ies/EpsUpdateResult.hpp"
#include "lte/gateway/c/core/oai/tasks/nas/ies/EpsUpdateType.hpp"
#include "lte/gateway/c/core/oai/tasks/nas/ies/MobileStationClassmark2.hpp"
#include "lte/gateway/c/core/oai/tasks/nas/ies/TrackingAreaIdentityList.hpp"
#include "lte/gateway/c/core/oai/tasks/nas/nas_procedures.hpp"
#include "lte/gateway/c/core/oai/tasks/nas/util/nas_timer.hpp"
#include "orc8r/gateway/c/common/service303/MetricsHelpers.hpp"
/****************************************************************************/
/****************  E X T E R N A L    D E F I N I T I O N S  ****************/
/****************************************************************************/
/****************************************************************************/
/*******************  L O C A L    D E F I N I T I O N S  *******************/
/****************************************************************************/

/* TODO Commented some function declarations below since these were called
 * from the code that got removed from TAU request handling function.
 * Reason this code was removed: This portion of code was incomplete and was
 * related to handling of some optional IEs /scenarios that were not relevant
 * for the TAU periodic update handling and might have resulted in
 * unexpected behaviour/instability.
 * At present support for TAU is limited to handling of periodic TAU request
 * only  mandatory IEs .
 * Other aspects of TAU are TODOs for future.
 */

static status_code_e emm_tracking_area_update_reject(
    const mme_ue_s1ap_id_t ue_id, const int emm_cause);
static status_code_e emm_tracking_area_update_accept(
    nas_emm_tau_proc_t* const tau_proc);
static int emm_tracking_area_update_abort(struct emm_context_s* emm_context,
                                          struct nas_base_proc_s* base_proc);

static nas_emm_tau_proc_t* emm_proc_create_procedure_tau(
    ue_mm_context_t* const ue_mm_context, emm_tau_request_ies_t* const ies);

static int send_tau_accept_and_check_for_neaf_flag(
    nas_emm_tau_proc_t* tau_proc, ue_mm_context_t* ue_mm_context);
/****************************************************************************/
/******************  E X P O R T E D    F U N C T I O N S  ******************/
/****************************************************************************/

status_code_e emm_proc_tracking_area_update_accept(
    nas_emm_tau_proc_t* const tau_proc) {
  status_code_e rc = RETURNerror;
  OAILOG_FUNC_IN(LOG_NAS_EMM);
  rc = emm_tracking_area_update_accept(tau_proc);
  OAILOG_FUNC_RETURN(LOG_NAS_EMM, rc);
}

/****************************************************************************
 **                                                                        **
 ** Name:        _csfb_handle_tracking_area_req()                          **
 **                                                                        **
 ** Description:                                                           **
 **                                                                        **
 ** Inputs:  emm_context_p:  Pointer to EMM context                        **
 **          emm_tau_request_ies_t: TAU Request received from UE           **
 **                                                                        **
 ** Outputs: Return:    RETURNok, RETURNerror                              **
 **                                                                        **
 ***************************************************************************/
status_code_e csfb_handle_tracking_area_req(emm_context_t* emm_context_p,
                                            emm_tau_request_ies_t* ies) {
  OAILOG_FUNC_IN(LOG_NAS_EMM);
  ue_mm_context_t* ue_mm_context = NULL;
  ue_mm_context =
      PARENT_STRUCT(emm_context_p, struct ue_mm_context_s, emm_context);
  if (!ue_mm_context) {
    OAILOG_ERROR(LOG_NAS_EMM, "Got Invalid UE Context during TAU procedure \n");
    OAILOG_FUNC_RETURN(LOG_NAS_EMM, RETURNerror);
  }
  OAILOG_INFO(LOG_NAS_EMM,
              "EMM-PROC"
              "_csfb_handle_tracking_area_req for UE-ID:" MME_UE_S1AP_ID_FMT
              "\n",
              ue_mm_context->mme_ue_s1ap_id);
  /* If periodic TAU is received, send Location Update to MME
   * only if SGS Association is established
   */
  if ((EPS_UPDATE_TYPE_COMBINED_TA_LA_UPDATING ==
       ies->eps_update_type.eps_update_type_value) ||
      (EPS_UPDATE_TYPE_COMBINED_TA_LA_UPDATING_WITH_IMSI_ATTACH ==
       ies->eps_update_type.eps_update_type_value) ||
      ((EPS_UPDATE_TYPE_PERIODIC_UPDATING ==
        ies->eps_update_type.eps_update_type_value) &&
       emm_context_p->csfbparams.sgs_loc_updt_status == SUCCESS)) {
    // Store TAU update type in emm context
    emm_context_p->tau_updt_type = ies->eps_update_type.eps_update_type_value;
    // Store active flag
    emm_context_p->csfbparams.tau_active_flag =
        ies->eps_update_type.active_flag;
    // Store Additional Update
    if ((ies->additional_updatetype != NULL) &&
        (SMS_ONLY == *(ies->additional_updatetype))) {
      emm_context_p->additional_update_type = SMS_ONLY;
    }
    nas_emm_tau_proc_t* tau_proc =
        get_nas_specific_procedure_tau(emm_context_p);
    if (!tau_proc) {
      tau_proc = emm_proc_create_procedure_tau(ue_mm_context, ies);
      if (!tau_proc) {
        OAILOG_ERROR_UE(LOG_MME_APP, emm_context_p->_imsi64,
                        "Failed to create new tau_proc for "
                        "ue_id" MME_UE_S1AP_ID_FMT "\n",
                        ue_mm_context->mme_ue_s1ap_id);
        OAILOG_FUNC_RETURN(LOG_NAS_EMM, RETURNerror);
      }
      if (ue_mm_context->sgs_context &&
          ((emm_context_p->tau_updt_type ==
            EPS_UPDATE_TYPE_COMBINED_TA_LA_UPDATING) ||
           (emm_context_p->tau_updt_type ==
            EPS_UPDATE_TYPE_PERIODIC_UPDATING))) {
        if ((ue_mm_context->sgs_context->vlr_reliable == true) &&
            (ue_mm_context->sgs_context->sgs_state == SGS_ASSOCIATED)) {
          OAILOG_INFO(LOG_MME_APP,
                      "Do not send Location Update Request to MSC\n");
          /* No need to send Location Update Request as SGS state is in
           * associated state and vlr_reliable flag is true
           * Send TAU accept to UE
           */
          send_tau_accept_and_check_for_neaf_flag(tau_proc, ue_mm_context);
          OAILOG_FUNC_RETURN(LOG_NAS_EMM, RETURNok);
        } else {
          if ((mme_app_handle_nas_cs_domain_location_update_req(
                  ue_mm_context, TRACKING_AREA_UPDATE_REQUEST)) !=
              RETURNerror) {
            OAILOG_ERROR(
                LOG_MME_APP,
                "Failed to send SGS Location Update Request to MSC for "
                "ue_id" MME_UE_S1AP_ID_FMT "\n",
                ue_mm_context->mme_ue_s1ap_id);
            OAILOG_FUNC_RETURN(LOG_NAS_EMM, RETURNerror);
          }
          OAILOG_FUNC_RETURN(LOG_NAS_EMM, RETURNok);
        }
      }
      OAILOG_FUNC_RETURN(LOG_NAS_EMM, RETURNok);
    }
  }
  OAILOG_FUNC_RETURN(LOG_NAS_EMM, RETURNerror);
}

/*
 * This function validates the eps_bearer_cntxt_status IE
 * received in TAU request and initiates
 * bearer deactivation if the bearers are deleted in UE
 */
static status_code_e handle_and_fill_eps_bearer_cntxt_status(
    const eps_bearer_context_status_t* const
        rcvd_tau_req_eps_eps_ber_cntx_status,
    ue_mm_context_t* ue_mm_context) {
  OAILOG_FUNC_IN(LOG_NAS_EMM);
  ebi_t ebi = 0;
  uint8_t pos = 0;
  uint8_t shift_bits = 8;
  pdn_cid_t pid = 0;
  bool is_ebi_active = false;

  for (uint8_t itrn = ESM_EBI_MIN; itrn < ESM_EBI_MAX; itrn++) {
    bearer_context_t* bearer_context =
        mme_app_get_bearer_context(ue_mm_context, itrn);

    if ((bearer_context) &&
        bearer_context->esm_ebr_context.status == ESM_EBR_ACTIVE) {
      ebi = bearer_context->ebi;
      /* Check if the ebi bit is in octet3 or octet4 of the rcvd TAU req
       * Ref : Spec 24.301 section 9.9.2.1
       */
      if ((ebi >= ESM_EBI_MIN) && (ebi <= (ESM_EBI_MAX / 2))) {
        pos = ebi + shift_bits;  // Octet 3
        is_ebi_active = (*rcvd_tau_req_eps_eps_ber_cntx_status) & (1 << pos);
      } else {
        pos = ebi - shift_bits;  // Octet 4
        is_ebi_active = (*rcvd_tau_req_eps_eps_ber_cntx_status) & (1 << pos);
      }
      /* Delete the bearer context if the bearer id rcvd in TAU req
       * is set to 0
       */
      pid = ue_mm_context->bearer_contexts[EBI_TO_INDEX(ebi)]->pdn_cx_id;
      if (pid >= MAX_APN_PER_UE) {
        OAILOG_ERROR_UE(LOG_NAS_EMM, ue_mm_context->emm_context._imsi64,
                        "No PDN connection found for pid=%d, EBI=%d \n", pid,
                        ebi);
        OAILOG_FUNC_RETURN(LOG_NAS_EMM, RETURNerror);
      }
      pdn_context_t* pdn_context = ue_mm_context->pdn_contexts[pid];
      if (!pdn_context) {
        OAILOG_ERROR_UE(LOG_NAS_EMM, ue_mm_context->emm_context._imsi64,
                        "PDN context is NULL for pid=%d, EBI=%d \n", pid, ebi);
        OAILOG_FUNC_RETURN(LOG_NAS_EMM, RETURNerror);
      }
      if (!is_ebi_active) {
        /* Deactivate the bearer as UE has locally deactivated it.
         * If its a default bearer, trigger delete session request
         * else trigger deactivate dedicated bearer request
         */
        if (ebi == pdn_context->default_ebi) {
          mme_app_send_delete_session_request(ue_mm_context, ebi, pid,
                                              true /*no_delete_gtpv2c_tunnel*/);
          ue_mm_context->nb_delete_sessions++;
          pdn_context->session_deletion_triggered = true;
        } else {
          /* If default bearer deletion is already triggered for this dedicated
           * bearer, no need to trigger bearer deactivation explicitly as it
           * will be deactivated as part of default bearer deletion
           */
          if (!pdn_context->session_deletion_triggered) {
            /* Store the bearers to be deleted in pdn context as
             * delete bearer cmd has to be sent to spgw per PDN
             */
            pdn_context->ebi_to_be_del[pdn_context->num_ebi_to_be_del++] = ebi;
            if (!ue_mm_context->mme_initiated_ded_bearer_deactivation) {
              ue_mm_context->mme_initiated_ded_bearer_deactivation = true;
            }
          }
        }
      } else {
        // Set the status of active bearers to be sent in TAU Accept
        (ue_mm_context->tau_accept_eps_ber_cntx_status) |= 1 << pos;
      }
    }
  }
  if (ue_mm_context->mme_initiated_ded_bearer_deactivation) {
    for (uint8_t pid = 0; pid < MAX_APN_PER_UE; pid++) {
      if (ue_mm_context->pdn_contexts[pid] &&
          (ue_mm_context->pdn_contexts[pid]->num_ebi_to_be_del > 0)) {
        mme_app_send_deactivate_dedicated_bearer_request(
            ue_mm_context, ue_mm_context->pdn_contexts[pid]);
        increment_counter("mme_initiated_dedicated_bearer_deactivation", 1, 2,
                          "result", "success", "cause", "local_deactivation");
      }
    }
  }
  OAILOG_FUNC_RETURN(LOG_NAS_EMM, RETURNok);
}

status_code_e emm_proc_tracking_area_update_request(
    const mme_ue_s1ap_id_t ue_id, emm_tau_request_ies_t* ies, int* emm_cause,
    tai_t tai) {
  OAILOG_FUNC_IN(LOG_NAS_EMM);
  status_code_e rc = RETURNerror;
  ue_mm_context_t* ue_mm_context = NULL;
  emm_context_t* emm_context = NULL;

  *emm_cause = EMM_CAUSE_SUCCESS;
  /*
   * Get the UE's EMM context if it exists
   */

  ue_mm_context = mme_ue_context_exists_mme_ue_s1ap_id(ue_id);
  if (ue_mm_context) {
    emm_context = &ue_mm_context->emm_context;
  }

  // May be the MME APP module did not find the context, but if we have the
  // GUTI, we may find it
  if (!ue_mm_context) {
    if (INVALID_M_TMSI != ies->old_guti.m_tmsi) {
      mme_app_desc_t* mme_app_desc_p = get_mme_nas_state(false);
      ue_mm_context = mme_ue_context_exists_guti(
          &mme_app_desc_p->mme_ue_contexts, &ies->old_guti);

      if (ue_mm_context) {
        emm_context = &ue_mm_context->emm_context;
        free_emm_tau_request_ies(&ies);
        OAILOG_DEBUG(LOG_NAS_EMM, "EMM-PROC-  GUTI Context found\n");
      } else {
        // NO S10
        rc = emm_tracking_area_update_reject(ue_id,
                                             EMM_CAUSE_IMPLICITLY_DETACHED);
        increment_counter("tracking_area_update_req", 1, 2, "result", "failure",
                          "cause", "ue_identify_cannot_be_derived");
        free_emm_tau_request_ies(&ies);
        OAILOG_FUNC_RETURN(LOG_NAS_EMM, rc);
      }
    }
    OAILOG_FUNC_RETURN(LOG_NAS_EMM, rc);
  }

  OAILOG_INFO(
      LOG_NAS_EMM,
      "EMM-PROC-  Tracking Area Update request. TAU_Type=%d, active_flag=%d, "
      "ue id " MME_UE_S1AP_ID_FMT "\n",
      ies->eps_update_type.eps_update_type_value,
      ies->eps_update_type.active_flag, ue_id);

  if (IS_EMM_CTXT_PRESENT_SECURITY(emm_context)) {
    emm_context->_security.kenb_ul_count = emm_context->_security.ul_count;
    if (ies->is_initial) {
      emm_context->_security.next_hop_chaining_count = 0;
    }
  }
  /* Check if it is not periodic update and not combined TAU for CSFB.
   * If we receive combined TAU/TAU with IMSI attach send Location Update Req
   * to MME instead of sending TAU accept immediately. After receiving Location
   * Update Accept from MME, send TAU accept
   */
  if ((_esm_data.conf.features & MME_API_CSFB_SMS_SUPPORTED) ||
      (_esm_data.conf.features & MME_API_SMS_SUPPORTED)) {
    if ((csfb_handle_tracking_area_req(emm_context, ies)) == RETURNok) {
      OAILOG_FUNC_RETURN(LOG_NAS_EMM, RETURNok);
    }
  }
  /*
   * Requirements MME24.301R10_5.5.3.2.4_3
   */
  if (ies->is_ue_radio_capability_information_update_needed) {
    OAILOG_DEBUG(LOG_NAS_EMM, "UE context exists: %s\n",
                 ue_mm_context ? "yes" : "no");
    if (ue_mm_context) {
      // Note: this is safe from double-free errors because it sets to NULL
      // after freeing, which free treats as a no-op.
      bdestroy_wrapper(&ue_mm_context->ue_radio_capability);
    }
  }
  /*
   * Store the mobile station classmark2 information recieved in Tracking Area
   * Update request This wil be required for SMS and SGS service request
   * procedure
   */
  if (ies->mobile_station_classmark2) {
    emm_context->_mob_st_clsMark2.revisionlevel =
        ies->mobile_station_classmark2->revisionlevel;
    emm_context->_mob_st_clsMark2.esind = ies->mobile_station_classmark2->esind;
    emm_context->_mob_st_clsMark2.a51 = ies->mobile_station_classmark2->a51;
    emm_context->_mob_st_clsMark2.rfpowercapability =
        ies->mobile_station_classmark2->rfpowercapability;
    emm_context->_mob_st_clsMark2.pscapability =
        ies->mobile_station_classmark2->pscapability;
    emm_context->_mob_st_clsMark2.ssscreenindicator =
        ies->mobile_station_classmark2->ssscreenindicator;
    emm_context->_mob_st_clsMark2.smcapability =
        ies->mobile_station_classmark2->smcapability;
    emm_context->_mob_st_clsMark2.vbs = ies->mobile_station_classmark2->vbs;
    emm_context->_mob_st_clsMark2.vgcs = ies->mobile_station_classmark2->vgcs;
    emm_context->_mob_st_clsMark2.fc = ies->mobile_station_classmark2->fc;
    emm_context->_mob_st_clsMark2.cm3 = ies->mobile_station_classmark2->cm3;
    emm_context->_mob_st_clsMark2.lcsvacap =
        ies->mobile_station_classmark2->lcsvacap;
    emm_context->_mob_st_clsMark2.ucs2 = ies->mobile_station_classmark2->ucs2;
    emm_context->_mob_st_clsMark2.solsa = ies->mobile_station_classmark2->solsa;
    emm_context->_mob_st_clsMark2.cmsp = ies->mobile_station_classmark2->cmsp;
    emm_context->_mob_st_clsMark2.a53 = ies->mobile_station_classmark2->a53;
    emm_context->_mob_st_clsMark2.a52 = ies->mobile_station_classmark2->a52;
    emm_ctx_set_attribute_present(emm_context,
                                  EMM_CTXT_MEMBER_MOB_STATION_CLSMARK2);
  }

  /*
   * Requirement MME24.301R10_5.5.3.2.4_6
   */
  // If CSFB feature is not enabled, send TAU accept
  if ((EPS_UPDATE_TYPE_PERIODIC_UPDATING ==
       ies->eps_update_type.eps_update_type_value) ||
      (EPS_UPDATE_TYPE_TA_UPDATING ==
       ies->eps_update_type.eps_update_type_value) ||
      ((EPS_UPDATE_TYPE_COMBINED_TA_LA_UPDATING ==
        ies->eps_update_type.eps_update_type_value) &&
       (mme_config.accept_combined_attach_tau_wo_csfb))) {
    /*
     * MME24.301R10_5.5.3.2.4_6 Normal and periodic tracking area updating
     * procedure accepted by the network UE - EPS update type If the EPS update
     * type IE included in the TRACKING AREA UPDATE REQUEST message indicates
     * "periodic updating", and the UE was previously successfully attached for
     * EPS and non-EPS services, subject to operator policies the MME should
     * allocate a TAI list that does not span more than one location area.
     */
    // Handle TAU
    if (ue_mm_context->num_reg_sub > 0) {
      if (verify_service_area_restriction(tai.tac, ue_mm_context->reg_sub,
                                          ue_mm_context->num_reg_sub) !=
          RETURNok) {
        OAILOG_ERROR_UE(
            LOG_MME_APP, ue_mm_context->emm_context._imsi64,
            "No suitable cells found for tac = %d, sending tau_reject "
            "message "
            "for ue_id " MME_UE_S1AP_ID_FMT " with emm cause = %d\n",
            tai.tac, ue_mm_context->mme_ue_s1ap_id,
            EMM_CAUSE_NO_SUITABLE_CELLS);
        free_emm_tau_request_ies(&ies);
        if (emm_tracking_area_update_reject(ue_mm_context->mme_ue_s1ap_id,
                                            EMM_CAUSE_NO_SUITABLE_CELLS) !=
            RETURNok) {
          OAILOG_ERROR_UE(LOG_MME_APP, ue_mm_context->emm_context._imsi64,
                          "Sending of tau reject message failed for "
                          "ue_id " MME_UE_S1AP_ID_FMT "\n",
                          ue_mm_context->mme_ue_s1ap_id);
          OAILOG_FUNC_RETURN(LOG_MME_APP, RETURNerror);
        }
        OAILOG_FUNC_RETURN(LOG_MME_APP, RETURNok);
      }
    }
    if (verify_tau_tai(ue_mm_context->emm_context._imsi64,
                       ue_mm_context->emm_context._guti, tai,
                       &ue_mm_context->emm_context._tai_list) != RETURNok) {
      OAILOG_ERROR_UE(LOG_MME_APP, ue_mm_context->emm_context._imsi64,
                      "tac = %d not configured, sending tau_reject "
                      "message "
                      "for ue_id " MME_UE_S1AP_ID_FMT " with emm cause = %d\n",
                      tai.tac, ue_mm_context->mme_ue_s1ap_id,
                      EMM_CAUSE_TA_NOT_ALLOWED);
      free_emm_tau_request_ies(&ies);
      if (emm_tracking_area_update_reject(ue_mm_context->mme_ue_s1ap_id,
                                          EMM_CAUSE_TA_NOT_ALLOWED) !=
          RETURNok) {
        OAILOG_ERROR_UE(LOG_MME_APP, ue_mm_context->emm_context._imsi64,
                        "Sending of tau reject message failed for "
                        "ue_id " MME_UE_S1AP_ID_FMT "\n",
                        ue_mm_context->mme_ue_s1ap_id);
        OAILOG_FUNC_RETURN(LOG_MME_APP, RETURNerror);
      }
      OAILOG_FUNC_RETURN(LOG_MME_APP, RETURNok);
    }
    nas_emm_tau_proc_t* tau_proc = get_nas_specific_procedure_tau(emm_context);
    if (!tau_proc) {
      tau_proc = emm_proc_create_procedure_tau(ue_mm_context, ies);
      if (tau_proc) {
        // Store the received voice domain pref & UE usage setting IE
        if (ies->voicedomainpreferenceandueusagesetting) {
          memcpy(&emm_context->volte_params
                      .voice_domain_preference_and_ue_usage_setting,
                 ies->voicedomainpreferenceandueusagesetting,
                 sizeof(voice_domain_preference_and_ue_usage_setting_t));
        }
        // Send eps_bearer_context_status in TAU Accept if received in TAU Req
        if (tau_proc->ies->eps_bearer_context_status) {
          if (*tau_proc->ies->eps_bearer_context_status > 0) {
            if (handle_and_fill_eps_bearer_cntxt_status(
                    tau_proc->ies->eps_bearer_context_status, ue_mm_context) !=
                RETURNok) {
              OAILOG_ERROR_UE(
                  LOG_NAS_EMM, ue_mm_context->emm_context._imsi64,
                  "EMM-PROC- Handling of eps_bearer_cntxt_status failed for "
                  "ue_id=" MME_UE_S1AP_ID_FMT ")\n",
                  ue_id);
              OAILOG_FUNC_RETURN(LOG_NAS_EMM, RETURNerror);
            } else {
              /* If delete_session request is sent i.e. if default
               * bearer deletion is triggered or dedicated bearer deactivation
               * is triggered in
               * handle_and_fill_eps_bearer_cntxt_status,
               * wait for response from spgw and then send TAU accept.
               */
              if (ue_mm_context->nb_delete_sessions ||
                  (ue_mm_context->nb_delete_bearer_cmd)) {
                OAILOG_FUNC_RETURN(LOG_NAS_EMM, RETURNok);
              }
            }
          } else {
            OAILOG_WARNING_UE(
                LOG_NAS_EMM, ue_mm_context->emm_context._imsi64,
                "EMM-PROC- Received eps_bearer_cntxt_status IE in TAU req with "
                "value 0, ignoring the IE for "
                "ue_id=" MME_UE_S1AP_ID_FMT ")\n",
                ue_id);
          }
        }
        OAILOG_INFO_UE(LOG_NAS_EMM, ue_mm_context->emm_context._imsi64,
                       "EMM-PROC- Sending Tracking Area Update Accept. "
                       "ue_id=" MME_UE_S1AP_ID_FMT ", active flag=%d)\n",
                       ue_id, ies->eps_update_type.active_flag);

        rc = emm_tracking_area_update_accept(tau_proc);
        if (rc != RETURNok) {
          OAILOG_ERROR(
              LOG_NAS_EMM,
              "EMM-PROC- Processing Tracking Area Update Accept failed for "
              "ue_id=" MME_UE_S1AP_ID_FMT ")\n",
              ue_id);
          OAILOG_FUNC_RETURN(LOG_NAS_EMM, RETURNerror);
        }
        increment_counter("tracking_area_update_req", 1, 1, "result",
                          "success");
        OAILOG_FUNC_RETURN(LOG_NAS_EMM, rc);
      } else {
        OAILOG_ERROR(LOG_NAS_EMM,
                     "EMM-PROC- Failed to create EMM specific proc"
                     "for TAU for ue_id= " MME_UE_S1AP_ID_FMT ")\n",
                     ue_id);
      }
    }
  }

  free_emm_tau_request_ies(&ies);
  OAILOG_FUNC_RETURN(LOG_NAS_EMM, rc);
}
/****************************************************************************
 **                                                                        **
 ** Name:        emm_proc_tracking_area_update_reject()                    **
 **                                                                        **
 ** Description:                                                           **
 **                                                                        **
 ** Inputs:  ue_id:              UE lower layer identifier                  **
 **                  emm_cause: EMM cause code to be reported              **
 **                  Others:    None                                       **
 **                                                                        **
 ** Outputs:     None                                                      **
 **                  Return:    RETURNok, RETURNerror                      **
 **                  Others:    _emm_data                                  **
 **                                                                        **
 ***************************************************************************/
status_code_e emm_proc_tracking_area_update_reject(const mme_ue_s1ap_id_t ue_id,
                                                   const int emm_cause) {
  OAILOG_FUNC_IN(LOG_NAS_EMM);
  OAILOG_FUNC_RETURN(LOG_NAS_EMM,
                     emm_tracking_area_update_reject(ue_id, emm_cause));
}

/****************************************************************************/
/*********************  L O C A L    F U N C T I O N S  *********************/
/****************************************************************************/
/* TODO - Compiled out this function to remove compiler warnings since we
 * don't expect TAU Complete from UE as we dont support implicit
 * GUTI re-allocation during TAU procedure.
 */
#if 0
static int _emm_tracking_area_update (void *args)
...
#endif
/*
 * --------------------------------------------------------------------------
 * Timer handlers
 * --------------------------------------------------------------------------
 */

/** \fn status_code_e mme_app_handle_tau_t3450_expiry(
    zloop_t* loop, int timer_id, void* args){
\brief T3450 timeout handler
On the first expiry of the timer, the network shall retransmit the TRACKING AREA
UPDATE ACCEPT message and shall reset and restart timer T3450. The
retransmission is performed four times, i.e. on the fifth expiry of timer T3450,
the tracking area updating procedure is aborted. Both, the old and the new GUTI
shall be considered as valid until the old GUTI can be considered as invalid by
the network (see subclause 5.4.1.4). During this period the network acts as
described for case a above.
@param [in]args unused
*/
//------------------------------------------------------------------------------
status_code_e mme_app_handle_tau_t3450_expiry(zloop_t* loop, int timer_id,
                                              void* args) {
  OAILOG_FUNC_IN(LOG_NAS_EMM);
  mme_ue_s1ap_id_t mme_ue_s1ap_id = 0;
  if (!mme_pop_timer_arg_ue_id(timer_id, &mme_ue_s1ap_id)) {
    OAILOG_WARNING(LOG_NAS_EMM, "Invalid Timer Id expiration, Timer Id: %u\n",
                   timer_id);
    OAILOG_FUNC_RETURN(LOG_NAS_EMM, RETURNok);
  }

  struct ue_mm_context_s* ue_context_p = mme_app_get_ue_context_for_timer(
      mme_ue_s1ap_id, const_cast<char*>("Authentication T3450 Timer"));
  if (ue_context_p == NULL) {
    OAILOG_ERROR(
        LOG_MME_APP,
        "Invalid UE context received, MME UE S1AP Id: " MME_UE_S1AP_ID_FMT "\n",
        mme_ue_s1ap_id);
    OAILOG_FUNC_RETURN(LOG_NAS_EMM, RETURNok);
  }

  emm_context_t* emm_context = &ue_context_p->emm_context;

  if (!(emm_context)) {
    OAILOG_ERROR(LOG_NAS_EMM, "T3450 timer expired No EMM context\n");
    OAILOG_FUNC_RETURN(LOG_NAS_EMM, RETURNok);
  }
  nas_emm_tau_proc_t* tau_proc = get_nas_specific_procedure_tau(emm_context);

  if (tau_proc) {
    // Requirement MME24.301R10_5.5.3.2.7_c Abnormal cases on the network side -
    // T3450 time-out
    /*
     * Increment the retransmission counter
     */
    tau_proc->retransmission_count += 1;
    tau_proc->T3450.id = NAS_TIMER_INACTIVE_ID;
    OAILOG_WARNING_UE(
        LOG_NAS_EMM, emm_context->_imsi64,
        "EMM-PROC  - T3450 timer expired, retransmission counter = %d for ue "
        "id " MME_UE_S1AP_ID_FMT "\n",
        tau_proc->retransmission_count, tau_proc->ue_id);
    /*
     * Get the UE's EMM context
     */

    if (tau_proc->retransmission_count < TAU_COUNTER_MAX) {
      /*
       * Send attach accept message to the UE
       */
      emm_tracking_area_update_accept(tau_proc);
    } else {
      /*
       * Abort the attach procedure
       */
      /*
       * Abort the security mode control procedure
       */
      emm_sap_t emm_sap = {};
      emm_sap.primitive = EMMREG_ATTACH_ABORT;
      emm_sap.u.emm_reg.ue_id = tau_proc->ue_id;
      emm_sap.u.emm_reg.ctx = emm_context;
      emm_sap.u.emm_reg.notify = true;
      emm_sap.u.emm_reg.free_proc = true;
      emm_sap.u.emm_reg.u.attach.is_emergency = false;
      emm_sap_send(&emm_sap);
    }
  }
  OAILOG_FUNC_RETURN(LOG_NAS_EMM, RETURNok);
}

/* TODO - Compiled out this function to remove compiler warnings since we don't
 * support reauthetication and change in security context during periodic TAU
 * procedure.
 */
#if 0
/** \fn void _emm_tracking_area_update_security(void *args);
    \brief Performs the tracking area update procedure not accepted by the network.
     @param [in]args UE EMM context data
     @returns status of operation
*/
//------------------------------------------------------------------------------
static int _emm_tracking_area_update_security (emm_context_t * emm_context)
...
#endif

/** \fn  _emm_tracking_area_update_reject();
    \brief Performs the tracking area update procedure not accepted by the
   network.
     @param [in]args UE EMM context data
     @returns RETURNok, RETURNerror
*/
//------------------------------------------------------------------------------
static status_code_e emm_tracking_area_update_reject(
    const mme_ue_s1ap_id_t ue_id, const int emm_cause)

{
  OAILOG_FUNC_IN(LOG_NAS_EMM);
  status_code_e rc = RETURNok;
  emm_sap_t emm_sap = {};
  ue_mm_context_t* ue_mm_context = NULL;
  emm_context_t* emm_context = NULL;

  OAILOG_WARNING(
      LOG_NAS_EMM,
      "EMM-PROC- Sending Tracking Area Update Reject. ue_id=" MME_UE_S1AP_ID_FMT
      ", cause=%d)\n",
      ue_id, emm_cause);
  /*
   * Notify EMM-AS SAP that Tracking Area Update Reject message has to be sent
   * onto the network
   */
  emm_sap.primitive = EMMAS_ESTABLISH_REJ;
  emm_sap.u.emm_as.u.establish.ue_id = ue_id;
  emm_sap.u.emm_as.u.establish.eps_id.guti = NULL;

  emm_sap.u.emm_as.u.establish.emm_cause = emm_cause;
  emm_sap.u.emm_as.u.establish.nas_info = EMM_AS_NAS_INFO_TAU;
  emm_sap.u.emm_as.u.establish.nas_msg = NULL;
  /*
   * Setup EPS NAS security data
   */
  ue_mm_context = mme_ue_context_exists_mme_ue_s1ap_id(ue_id);
  if (ue_mm_context) {
    emm_context = &ue_mm_context->emm_context;
  }

  if (emm_context) {
    emm_as_set_security_data(&emm_sap.u.emm_as.u.establish.sctx,
                             &emm_context->_security, false, false);
  } else {
    emm_as_set_security_data(&emm_sap.u.emm_as.u.establish.sctx, NULL, false,
                             false);
  }
  rc = emm_sap_send(&emm_sap);
  increment_counter("tracking_area_update", 1, 1, "action", "tau_reject_sent");

  OAILOG_FUNC_RETURN(LOG_NAS_EMM, rc);
}

//------------------------------------------------------------------------------

static int build_csfb_parameters_combined_tau(emm_context_t* emm_ctx,
                                              emm_as_establish_t* establish) {
  OAILOG_FUNC_IN(LOG_NAS_EMM);

  if ((emm_ctx->tau_updt_type == EPS_UPDATE_TYPE_COMBINED_TA_LA_UPDATING) ||
      (emm_ctx->tau_updt_type ==
       EPS_UPDATE_TYPE_COMBINED_TA_LA_UPDATING_WITH_IMSI_ATTACH) ||
      (emm_ctx->tau_updt_type == EPS_UPDATE_TYPE_PERIODIC_UPDATING)) {
    // Check if SGS Location update procedure is successful
    if (emm_ctx->csfbparams.sgs_loc_updt_status == SUCCESS) {
      if (emm_ctx->csfbparams.presencemask & LAI) {
        establish->location_area_identification = &emm_ctx->csfbparams.lai;
      }
      // Encode Mobile Identity
      if (emm_ctx->csfbparams.presencemask & MOBILE_IDENTITY) {
        establish->ms_identity = &emm_ctx->csfbparams.mobileid;
      }
      // Send Additional Update type if SMS_ONLY is enabled
      if ((emm_ctx->csfbparams.presencemask & ADD_UPDATE_TYPE) &&
          (emm_ctx->csfbparams.additional_updt_res ==
           ADDITIONAL_UPDT_RES_SMS_ONLY)) {
        establish->additional_update_result =
            &emm_ctx->csfbparams.additional_updt_res;
      }
      establish->eps_update_result = EPS_UPDATE_RESULT_COMBINED_TA_LA_UPDATED;
    } else if (emm_ctx->csfbparams.sgs_loc_updt_status == FAILURE) {
      establish->combined_tau_emm_cause = &emm_ctx->emm_cause;
      establish->eps_update_result = EPS_UPDATE_RESULT_TA_UPDATED;
    }
    OAILOG_FUNC_RETURN(LOG_NAS_EMM, RETURNok);
  }
  OAILOG_FUNC_RETURN(LOG_NAS_EMM, RETURNerror);
}

/** \fn void _emm_tracking_area_update_accept (emm_context_t *
   emm_context,tau_data_t * data); \brief Sends ATTACH ACCEPT message and start
   timer T3450.
     @param [in]emm_context UE EMM context data
     @param [in]data    UE TAU accept data
     @returns status of operation (RETURNok, RETURNerror)
*/
//------------------------------------------------------------------------------
static status_code_e emm_tracking_area_update_accept(
    nas_emm_tau_proc_t* const tau_proc) {
  OAILOG_FUNC_IN(LOG_NAS_EMM);
  status_code_e rc = RETURNerror;
  emm_sap_t emm_sap = {};
  ue_mm_context_t* ue_mm_context = NULL;
  emm_context_t* emm_context = NULL;
  uint8_t eps_update_result = 0;

  if ((tau_proc) && (tau_proc->ies)) {
    ue_mm_context = mme_ue_context_exists_mme_ue_s1ap_id(tau_proc->ue_id);
    if (ue_mm_context) {
      emm_context = &ue_mm_context->emm_context;
    } else {
      OAILOG_ERROR(LOG_NAS_EMM,
                   "Failed to get emm context for ue_id"
                   "" MME_UE_S1AP_ID_FMT " \n",
                   tau_proc->ue_id);
      OAILOG_FUNC_RETURN(LOG_NAS_EMM, rc);
    }

    switch (tau_proc->ies->eps_update_type.eps_update_type_value) {
      case EPS_UPDATE_TYPE_TA_UPDATING:
      case EPS_UPDATE_TYPE_PERIODIC_UPDATING:
        eps_update_result = EPS_UPDATE_RESULT_TA_UPDATED;
        break;
      case EPS_UPDATE_TYPE_COMBINED_TA_LA_UPDATING:
        eps_update_result = EPS_UPDATE_RESULT_COMBINED_TA_LA_UPDATED;
        break;
      default:
        eps_update_result = EPS_UPDATE_RESULT_TA_UPDATED;
        break;
    }

    if ((tau_proc->ies->eps_update_type.active_flag) &&
        (ue_mm_context->ecm_state != ECM_CONNECTED)) {
      /* If active flag is set to true in TAU request then re-establish bearer
       * also for the UE while sending TAU Accept message
       */
      emm_sap.primitive = EMMAS_ESTABLISH_CNF;
      emm_sap.u.emm_as.u.establish.ue_id = tau_proc->ue_id;

      emm_sap.u.emm_as.u.establish.eps_id.guti = &emm_context->_guti;
      emm_sap.u.emm_as.u.establish.new_guti = NULL;

      emm_sap.u.emm_as.u.establish.eps_update_result = eps_update_result;
      memcpy(&emm_sap.u.emm_as.u.establish.tai_list, &emm_context->_tai_list,
             sizeof(tai_list_t));
      emm_sap.u.emm_as.u.establish.nas_info = EMM_AS_NAS_INFO_TAU;

      // Send eps_bearer_context_status in TAU Accept if received in TAU Req
      if (tau_proc->ies->eps_bearer_context_status &&
          ue_mm_context->tau_accept_eps_ber_cntx_status) {
        emm_sap.u.emm_as.u.establish.eps_bearer_context_status =
            &ue_mm_context->tau_accept_eps_ber_cntx_status;
      }
      // TODO Reminder
      emm_sap.u.emm_as.u.establish.location_area_identification = NULL;
      emm_sap.u.emm_as.u.establish.combined_tau_emm_cause = NULL;

      emm_sap.u.emm_as.u.establish.t3423 = NULL;
      emm_sap.u.emm_as.u.establish.t3412 = NULL;
      emm_sap.u.emm_as.u.establish.t3402 = NULL;
      // TODO Reminder
      emm_sap.u.emm_as.u.establish.equivalent_plmns = NULL;
      emm_sap.u.emm_as.u.establish.emergency_number_list = NULL;

      emm_sap.u.emm_as.u.establish.eps_network_feature_support =
          reinterpret_cast<eps_network_feature_support_t*>(&_emm_data.conf
              .eps_network_feature_support);
      emm_sap.u.emm_as.u.establish.additional_update_result = NULL;
      emm_sap.u.emm_as.u.establish.t3412_extended = NULL;
      emm_sap.u.emm_as.u.establish.nas_msg =
          NULL;  // No ESM container message in TAU Accept message

      // If CSFB is enabled, encode LAI,Mobile Id and Additional Update Type
      if ((_esm_data.conf.features & MME_API_CSFB_SMS_SUPPORTED) ||
          (_esm_data.conf.features & MME_API_SMS_SUPPORTED)) {
        OAILOG_INFO(LOG_NAS_EMM,
                    "Encoding _build_csfb_parameters_combined_tau\n");
        if (build_csfb_parameters_combined_tau(
                emm_context, &emm_sap.u.emm_as.u.establish) == RETURNerror) {
          OAILOG_ERROR(
              LOG_NAS_EMM,
              "EMM-PROC  - Error in encoding Combined TAU parameters for CSFB"
              " %u\n",
              tau_proc->ue_id);
        }
      }
      /*
       * Setup EPS NAS security data
       */

      emm_as_set_security_data(&emm_sap.u.emm_as.u.establish.sctx,
                               &emm_context->_security, false, true);
      OAILOG_INFO(LOG_NAS_EMM, "EMM-PROC  - encryption = 0x%X\n",
                  emm_sap.u.emm_as.u.establish.encryption);
      OAILOG_INFO(LOG_NAS_EMM, "EMM-PROC  - integrity  = 0x%X\n",
                  emm_sap.u.emm_as.u.establish.integrity);
      emm_sap.u.emm_as.u.establish.encryption =
          emm_context->_security.selected_algorithms.encryption;
      emm_sap.u.emm_as.u.establish.integrity =
          emm_context->_security.selected_algorithms.integrity;
      OAILOG_INFO(LOG_NAS_EMM, "EMM-PROC  - encryption = 0x%X (0x%X)\n",
                  emm_sap.u.emm_as.u.establish.encryption,
                  emm_context->_security.selected_algorithms.encryption);
      OAILOG_INFO(LOG_NAS_EMM, "EMM-PROC  - integrity  = 0x%X (0x%X)\n",
                  emm_sap.u.emm_as.u.establish.integrity,
                  emm_context->_security.selected_algorithms.integrity);

      rc = emm_sap_send(&emm_sap);

      // Check if new TMSI is allocated as part of Combined TAU
      if (rc != RETURNerror) {
        if ((emm_sap.u.emm_as.u.establish.new_guti != NULL) ||
            (emm_context->csfbparams.newTmsiAllocated)) {
          /*
           * Re-start T3450 timer
           */
          nas_stop_T3450(tau_proc->ue_id, &tau_proc->T3450);
          nas_start_T3450(tau_proc->ue_id, &tau_proc->T3450,
                          tau_proc->emm_spec_proc.emm_proc.base_proc.time_out);
          increment_counter("tracking_area_update", 1, 1, "action",
                            " initial_ictr_tau_accept_sent");
          OAILOG_FUNC_RETURN(LOG_NAS_EMM, rc);
        }
      }
      nas_delete_tau_procedure(emm_context);
    }  // Active Flag
    else {
      /* If active flag is not set to true in TAU request then just send TAU
       * accept. After sending TAU accept initiate S1 context release procedure
       * for the UE if new GUTI is not sent in TAU accept message. Note - At
       * present implicit GUTI reallocation is not supported and hence GUTI is
       * not sent in TAU accept message.
       */
      emm_as_data_t* emm_as = &emm_sap.u.emm_as.u.data;

      /*
       * Setup NAS information message to transfer
       */
      emm_as->nas_info = EMM_AS_NAS_DATA_TAU;
      emm_as->nas_msg = NULL;  // No ESM container
      /*
       * Set the UE identifier
       */
      emm_as->ue_id = tau_proc->ue_id;
      emm_sap.u.emm_as.u.data.eps_update_result = eps_update_result;
      memcpy(&emm_sap.u.emm_as.u.data.tai_list, &emm_context->_tai_list,
             sizeof(tai_list_t));
      // Send eps_bearer_context_status in TAU Accept if received in TAU Req
      if (tau_proc->ies->eps_bearer_context_status &&
          ue_mm_context->tau_accept_eps_ber_cntx_status) {
        emm_as->eps_bearer_context_status =
            &ue_mm_context->tau_accept_eps_ber_cntx_status;
      }

      emm_sap.u.emm_as.u.data.eps_network_feature_support =
          reinterpret_cast<eps_network_feature_support_t*>(&_emm_data.conf
              .eps_network_feature_support);

      /*If CSFB is enabled,store LAI,Mobile Identity and
       * Additional Update type to be sent in TAU accept to S1AP
       */
      if ((_esm_data.conf.features & MME_API_CSFB_SMS_SUPPORTED) ||
          (_esm_data.conf.features & MME_API_SMS_SUPPORTED)) {
        if (emm_context->csfbparams.sgs_loc_updt_status == SUCCESS) {
          if (emm_context->csfbparams.presencemask & LAI) {
            emm_as->location_area_identification = &emm_context->csfbparams.lai;
          }
          if (emm_context->csfbparams.presencemask & MOBILE_IDENTITY) {
            emm_as->ms_identity = &emm_context->csfbparams.mobileid;
          }
          if (emm_context->csfbparams.presencemask & ADD_UPDATE_TYPE) {
            emm_as->additional_update_result =
                &emm_context->csfbparams.additional_updt_res;
          }
          emm_as->sgs_loc_updt_status = SUCCESS;
        } else if (emm_context->csfbparams.sgs_loc_updt_status == FAILURE) {
          emm_as->sgs_loc_updt_status = FAILURE;
          emm_as->sgs_reject_cause = reinterpret_cast<uint32_t*>(&emm_context->emm_cause);
        }
      }
      /*
       * Setup EPS NAS security data
       */
      emm_as_set_security_data(&emm_as->sctx, &emm_context->_security, false,
                               true);
      /*
       * Notify EMM-AS SAP that TAU Accept message has to be sent to the network
       */
      emm_sap.primitive = EMMAS_DATA_REQ;
      rc = emm_sap_send(&emm_sap);
      increment_counter("tracking_area_update", 1, 1, "action",
                        "tau_accept_sent");

      // Start T3450 timer if new TMSI is allocated
      if (emm_context->csfbparams.newTmsiAllocated) {
        if (tau_proc->T3450.id != NAS_TIMER_INACTIVE_ID) {
          /*
           * Re-start T3450 timer
           */
          nas_stop_T3450(tau_proc->ue_id, &tau_proc->T3450);
          nas_start_T3450(tau_proc->ue_id, &tau_proc->T3450,
                          tau_proc->emm_spec_proc.emm_proc.base_proc.time_out);
        } else {
          /*
           * Start T3450 timer
           */
          nas_start_T3450(tau_proc->ue_id, &tau_proc->T3450,
                          tau_proc->emm_spec_proc.emm_proc.base_proc.time_out);
        }

        OAILOG_INFO(LOG_NAS_EMM,
                    "EMM-PROC  - Timer T3450 %ld expires in %u"
                    " seconds (TAU) for ue id " MME_UE_S1AP_ID_FMT "\n",
                    tau_proc->T3450.id, tau_proc->T3450.msec, tau_proc->ue_id);
      } else {
        nas_delete_tau_procedure(emm_context);
      }
    }
  } else {
    OAILOG_WARNING(LOG_NAS_EMM, "EMM-PROC  - TAU procedure NULL");
  }
  OAILOG_FUNC_RETURN(LOG_NAS_EMM, rc);
}

//------------------------------------------------------------------------------
static int emm_tracking_area_update_abort(struct emm_context_s* emm_context,
                                          struct nas_base_proc_s* base_proc) {
  OAILOG_FUNC_IN(LOG_NAS_EMM);
  int rc = RETURNerror;

  if (emm_context) {
    nas_emm_tau_proc_t* tau_proc = get_nas_specific_procedure_tau(emm_context);

    if (tau_proc) {
      mme_ue_s1ap_id_t ue_id =
          PARENT_STRUCT(emm_context, struct ue_mm_context_s, emm_context)
              ->mme_ue_s1ap_id;
      OAILOG_WARNING(
          LOG_NAS_EMM,
          "EMM-PROC  - Abort the TAU procedure (ue_id=" MME_UE_S1AP_ID_FMT ")",
          ue_id);

      /*
       * Stop timer T3450
       */
      nas_stop_T3450(tau_proc->ue_id, &tau_proc->T3450);

      /*
       * Notify EMM that EPS attach procedure failed
       */
      emm_sap_t emm_sap = {};

      emm_sap.primitive = EMMREG_ATTACH_REJ;
      emm_sap.u.emm_reg.ue_id = ue_id;
      emm_sap.u.emm_reg.ctx = emm_context;
      rc = emm_sap_send(&emm_sap);
    }
  }
  OAILOG_FUNC_RETURN(LOG_NAS_EMM, rc);
}

//------------------------------------------------------------------------------
void free_emm_tau_request_ies(emm_tau_request_ies_t** const ies) {
  if ((*ies)->additional_guti) {
    free_wrapper(reinterpret_cast<void**>(&((*ies)->additional_guti)));
  }
  if ((*ies)->ue_network_capability) {
    free_wrapper(reinterpret_cast<void**>(&((*ies)->ue_network_capability)));
  }
  if ((*ies)->last_visited_registered_tai) {
    free_wrapper(reinterpret_cast<void**>(&((*ies)->last_visited_registered_tai)));
  }
  if ((*ies)->last_visited_registered_tai) {
    free_wrapper(reinterpret_cast<void**>(&((*ies)->last_visited_registered_tai)));
  }
  if ((*ies)->drx_parameter) {
    free_wrapper(reinterpret_cast<void**>(&((*ies)->drx_parameter)));
  }
  if ((*ies)->eps_bearer_context_status) {
    free_wrapper(reinterpret_cast<void**>(&((*ies)->eps_bearer_context_status)));
  }
  if ((*ies)->ms_network_capability) {
    free_wrapper(reinterpret_cast<void**>(&((*ies)->ms_network_capability)));
  }
  if ((*ies)->tmsi_status) {
    free_wrapper(reinterpret_cast<void**>(&((*ies)->tmsi_status)));
  }
  if ((*ies)->mobile_station_classmark2) {
    free_wrapper(reinterpret_cast<void**>(&((*ies)->mobile_station_classmark2)));
  }
  if ((*ies)->mobile_station_classmark3) {
    free_wrapper(reinterpret_cast<void**>(&((*ies)->mobile_station_classmark3)));
  }
  if ((*ies)->supported_codecs) {
    free_wrapper(reinterpret_cast<void**>(&((*ies)->supported_codecs)));
  }
  if ((*ies)->additional_updatetype) {
    free_wrapper(reinterpret_cast<void**>(&((*ies)->additional_updatetype)));
  }
  if ((*ies)->old_guti_type) {
    free_wrapper(reinterpret_cast<void**>(&((*ies)->old_guti_type)));
  }
  free_wrapper(reinterpret_cast<void**>(ies));
}

/****************************************************************************
 **                                                                        **
 ** Name:    emm_proc_tau_complete()                                       **
 **                                                                        **
 ** Description: Terminates the TAU procedure upon receiving TAU           **
 **      Complete message from the UE.                                     **
 **                                                                        **
 **              3GPP TS 24.301, section 5.5.1.2.4                         **
 **      Upon receiving an TAU COMPLETE message, the MME shall             **
 **      stop timer T3450,send S1 UE context release if Active flag is     **
 **      not set                                                           **
 **                                                                        **
 ** Inputs:  ue_id:      UE lower layer identifier                         **
 **      Others:    _emm_data                                              **
 **                                                                        **
 ** Outputs:     None                                                      **
 **      Return:    RETURNok, RETURNerror                                  **
 **      Others:    _emm_data, T3450                                       **
 **                                                                        **
 ***************************************************************************/
status_code_e emm_proc_tau_complete(mme_ue_s1ap_id_t ue_id) {
  emm_context_t* emm_ctx = NULL;
  struct ue_mm_context_s* ue_context_p = NULL;

  OAILOG_FUNC_IN(LOG_NAS_EMM);
  OAILOG_INFO(LOG_NAS_EMM,
              "EMM-PROC  - EPS TAU complete (ue_id=" MME_UE_S1AP_ID_FMT ")\n",
              ue_id);
  REQUIREMENT_3GPP_24_301(R10_5_5_1_2_4__20);
  /*
   * Release retransmission timer parameters
   */
  emm_proc_common_clear_args(ue_id);

  /*
   * Get the EMM context
   */
  emm_ctx = emm_context_get(&_emm_data, ue_id);

  if (emm_ctx) {
    /*
     * Upon receiving an TAU COMPLETE message, the MME shall stop timer T3450
     * Timer is stopped within nas_delete_tau_procedure()
     */
    nas_emm_tau_proc_t* tau_proc = get_nas_specific_procedure_tau(emm_ctx);
    if (tau_proc) {
      OAILOG_INFO(LOG_NAS_EMM,
                  "EMM-PROC  - Stop timer T3450 for ue id " MME_UE_S1AP_ID_FMT
                  "\n",
                  ue_id);
      if (emm_ctx->csfbparams.newTmsiAllocated) {
        nas_delete_tau_procedure(emm_ctx);
      }
    }
    // If Active flag is not set, initiate UE context release
    if (!emm_ctx->csfbparams.tau_active_flag) {
      ue_context_p =
          PARENT_STRUCT(emm_ctx, struct ue_mm_context_s, emm_context);
      ue_context_p->ue_context_rel_cause = S1AP_NAS_NORMAL_RELEASE;
      // Notify S1AP to send UE Context Release Command to eNB.
      mme_app_itti_ue_context_release(ue_context_p,
                                      ue_context_p->ue_context_rel_cause);
    }
  } else {
    OAILOG_ERROR(LOG_NAS_EMM,
                 "Failed to find emm context for ue_id received in TAU "
                 "Complete" MME_UE_S1AP_ID_FMT "\n",
                 ue_id);
    OAILOG_FUNC_RETURN(LOG_NAS_EMM, RETURNerror);
  }
  OAILOG_FUNC_RETURN(LOG_NAS_EMM, RETURNok);
}

static nas_emm_tau_proc_t* emm_proc_create_procedure_tau(
    ue_mm_context_t* const ue_mm_context, emm_tau_request_ies_t* const ies) {
  OAILOG_FUNC_IN(LOG_NAS_EMM);

  nas_emm_tau_proc_t* tau_proc =
      nas_new_tau_procedure(&ue_mm_context->emm_context);
  if ((tau_proc)) {
    tau_proc->ies = ies;
    tau_proc->ue_id = ue_mm_context->mme_ue_s1ap_id;
    tau_proc->emm_spec_proc.emm_proc.base_proc.abort =
        (proc_abort_t)emm_tracking_area_update_abort;
    tau_proc->emm_spec_proc.emm_proc.base_proc.fail_in =
        NULL;  // No parent procedure
    tau_proc->emm_spec_proc.emm_proc.base_proc.time_out =
        (time_out_t)mme_app_handle_tau_t3450_expiry;
    tau_proc->emm_spec_proc.emm_proc.base_proc.fail_out = NULL;
    OAILOG_FUNC_RETURN(LOG_NAS_EMM, tau_proc);
  }
  OAILOG_ERROR_UE(LOG_NAS_EMM, ue_mm_context->emm_context._imsi64,
                  "Failed to create tau_proc for ue_id " MME_UE_S1AP_ID_FMT
                  "\n",
                  ue_mm_context->mme_ue_s1ap_id);
  OAILOG_FUNC_RETURN(LOG_NAS_EMM, NULL);
}

/****************************************************************************
 **                                                                        **
 ** Name:        _send_tau_accept_and_check_for_neaf_flag()                **
 **                                                                        **
 ** Description:                                                           **
 **                                                                        **
 ** Inputs:  tau_proc: pointer for TAU emm specific proceddure             **
 **          ue_ctx:  UE context                                           **
 **                                                                        **
 ** Outputs: Return:    RETURNok, RETURNerror                              **
 **                                                                        **
 ***************************************************************************/
static int send_tau_accept_and_check_for_neaf_flag(
    nas_emm_tau_proc_t* tau_proc, ue_mm_context_t* ue_context) {
  OAILOG_FUNC_IN(LOG_NAS_EMM);
  if ((emm_proc_tracking_area_update_accept(tau_proc)) == RETURNerror) {
    OAILOG_ERROR(LOG_NAS_EMM,
                 "EMMCN-SAP  - "
                 "Failed to send TAU accept for UE id " MME_UE_S1AP_ID_FMT
                 " \n",
                 ue_context->mme_ue_s1ap_id);
    OAILOG_FUNC_RETURN(LOG_NAS_EMM, RETURNerror);
  }
  if (mme_ue_context_get_ue_sgs_neaf(ue_context->mme_ue_s1ap_id)) {
    OAILOG_INFO(LOG_MME_APP,
                "Sending UE Activity Ind to MSC for ue-id: " MME_UE_S1AP_ID_FMT
                "\n",
                ue_context->mme_ue_s1ap_id);
    /* neaf flag is true*/
    /* send the SGSAP Ue activity indication to MSC/VLR */
    char imsi_str[IMSI_BCD_DIGITS_MAX + 1];
    IMSI64_TO_STRING(ue_context->emm_context._imsi64, imsi_str,
                     ue_context->emm_context._imsi.length);
    mme_app_send_itti_sgsap_ue_activity_ind(imsi_str, strlen(imsi_str));
    mme_ue_context_update_ue_sgs_neaf(ue_context->mme_ue_s1ap_id, false);
  }
  OAILOG_FUNC_RETURN(LOG_NAS_EMM, RETURNok);
}

/****************************************************************************
 **                                                                        **
 ** Name:        send_tau_accept_with_eps_bearer_ctx_status                **
 **                                                                        **
 ** Description: Sends TAU accept message stored in ue context             **
 **                                                                        **
 ** Inputs:  ue_context:  UE context                                       **
 **                                                                        **
 ** Outputs: Return:    RETURNok, RETURNerror                              **
 **                                                                        **
 ***************************************************************************/
status_code_e send_tau_accept_with_eps_bearer_ctx_status(
    ue_mm_context_t* ue_context) {
  OAILOG_FUNC_IN(LOG_NAS_EMM);

  nas_emm_tau_proc_t* tau_proc =
      get_nas_specific_procedure_tau(&ue_context->emm_context);
  if (!tau_proc) {
    OAILOG_ERROR_UE(LOG_NAS_EMM, ue_context->emm_context._imsi64,
                    "tau_proc is NULL,failed to send TAU accept for UE "
                    "id " MME_UE_S1AP_ID_FMT " \n",
                    ue_context->mme_ue_s1ap_id);
    OAILOG_FUNC_RETURN(LOG_NAS_EMM, RETURNerror);
  }
  if (!ue_context->tau_accept_eps_ber_cntx_status) {
    OAILOG_ERROR_UE(
        LOG_NAS_EMM, ue_context->emm_context._imsi64,
        "tau_accept_eps_ber_cntx_status stored in ue_context is NULL,failed to "
        "send TAU accept for UE id " MME_UE_S1AP_ID_FMT " \n",
        ue_context->mme_ue_s1ap_id);
    free_emm_tau_request_ies(&tau_proc->ies);
    OAILOG_FUNC_RETURN(LOG_NAS_EMM, RETURNerror);
  }

  if (emm_tracking_area_update_accept(tau_proc) != RETURNok) {
    OAILOG_ERROR_UE(
        LOG_NAS_EMM, ue_context->emm_context._imsi64,
        "EMM-PROC- Sending of Tracking Area Update Accept failed for "
        "ue_id=" MME_UE_S1AP_ID_FMT ")\n",
        ue_context->mme_ue_s1ap_id);
    free_emm_tau_request_ies(&tau_proc->ies);
    OAILOG_FUNC_RETURN(LOG_NAS_EMM, RETURNerror);
  }

  OAILOG_INFO_UE(LOG_NAS_EMM, ue_context->emm_context._imsi64,
                 "Sent TAU with tau_accept_eps_ber_cntx_status for UE "
                 "id " MME_UE_S1AP_ID_FMT " \n",
                 ue_context->mme_ue_s1ap_id);

  increment_counter("tracking_area_update_req", 1, 1, "result", "success");
  OAILOG_FUNC_RETURN(LOG_NAS_EMM, RETURNok);
}
