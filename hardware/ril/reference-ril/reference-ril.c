/* //device/system/reference-ril/reference-ril.c
**
** Copyright 2006, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <alloca.h>
#include "atchannel.h"
#include "at_tok.h"
#include "at_error.h"
#include "misc.h"
#include <getopt.h>
#include <sys/socket.h>
#include <cutils/sockets.h>
#include <termios.h>
#include <sys/system_properties.h>

#include "../include/telephony/ril.h"
#include "hardware/qemu_pipe.h"

#define LOG_TAG "RIL"
#include <utils/Log.h>

#include <cutils/properties.h>

#define MAX_AT_RESPONSE 0x1000

/* pathname returned from RIL_REQUEST_SETUP_DATA_CALL / RIL_REQUEST_SETUP_DEFAULT_PDP */
#define PPP_TTY_PATH "ppp0"

#define PPPD_EXIT_CODE           "net.gprs.ppp-exit"

#define PPP_START_RETRY_TIME  5
#define PPP_STOP_RETRY_TIME  5

/**
 *  Wythe:Add on 2013-04-02 for quectel 4.0 ril
 */
#define __QUECTEL_UC20__

//  add by wythe on 2013-9-11
//  macro for Quectel Ril Version.
#define REFERENCE_RIL_VERSION   "Quectel-Ril 1.5.0"

#ifndef LOGD
#define LOGD ALOGD
#endif

#ifndef LOGI
#define LOGI ALOGI
#endif

#ifndef LOGE
#define LOGE ALOGE
#endif

//  Delete by wythe on 2014-1-26
#if 0
typedef enum {
    SIM_ABSENT = 0,
    SIM_NOT_READY = 1,
    SIM_READY = 2, /* SIM_READY means the radio state is RADIO_STATE_SIM_READY */
    SIM_PIN = 3,
    SIM_PUK = 4,
    SIM_NETWORK_PERSONALIZATION = 5
} SIM_Status;
#endif

static void onRequest (int request, void *data, size_t datalen, RIL_Token t);
static RIL_RadioState currentState();
static int onSupports (int requestCode);
static void onCancel (RIL_Token t);
static const char *getVersion();
static int isRadioOn();
//  Delete by wythe on 2014-1-26
#if 0
static SIM_Status getSIMStatus();
#endif
static int getCardStatus(RIL_CardStatus_v6 **pp_card_status);
static void freeCardStatus(RIL_CardStatus_v6 *p_card_status);
static void onDataCallListChanged(void *param);

extern const char * requestToString(int request);

/*** Static Variables ***/
static const RIL_RadioFunctions s_callbacks = {
    RIL_VERSION,
    onRequest,
    currentState,
    onSupports,
    onCancel,
    getVersion
};

#ifdef RIL_SHLIB
static const struct RIL_Env *s_rilenv;

#define RIL_onRequestComplete(t, e, response, responselen) s_rilenv->OnRequestComplete(t,e, response, responselen)
#define RIL_onUnsolicitedResponse(a,b,c) s_rilenv->OnUnsolicitedResponse(a,b,c)
#define RIL_requestTimedCallback(a,b,c) s_rilenv->RequestTimedCallback(a,b,c)
#endif

static RIL_RadioState sState = RADIO_STATE_UNAVAILABLE;

static pthread_mutex_t s_state_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t s_state_cond = PTHREAD_COND_INITIALIZER;

static int s_port = -1;
static const char * s_device_path = NULL;
static int          s_device_socket = 0;

/* trigger change to this with s_state_cond */
static int s_closed = 0;

static int sFD;     /* file desc of AT channel */
static char sATBuffer[MAX_AT_RESPONSE+1];
static char *sATBufferCur = NULL;

static const struct timeval TIMEVAL_SIMPOLL = {1,0};
static const struct timeval TIMEVAL_CALLSTATEPOLL = {0,500000};
static const struct timeval TIMEVAL_0 = {0,0};

static void pollSIMState (void *param);
static void setRadioState(RIL_RadioState newState);

static int clccStateToRILState(int state, RIL_CallState *p_state)

{
    switch(state) {
        case 0: *p_state = RIL_CALL_ACTIVE;   return 0;
        case 1: *p_state = RIL_CALL_HOLDING;  return 0;
        case 2: *p_state = RIL_CALL_DIALING;  return 0;
        case 3: *p_state = RIL_CALL_ALERTING; return 0;
        case 4: *p_state = RIL_CALL_INCOMING; return 0;
        case 5: *p_state = RIL_CALL_WAITING;  return 0;
        default: return -1;
    }
}

/**
 * Note: directly modified line and has *p_call point directly into
 * modified line
 */
static int callFromCLCCLine(char *line, RIL_Call *p_call)
{
        //+CLCC: 1,0,2,0,0,\"+18005551212\",145
        //     index,isMT,state,mode,isMpty(,number,TOA)?

    int err;
    int state;
    int mode;

    err = at_tok_start(&line);
    if (err < 0) goto error;

    err = at_tok_nextint(&line, &(p_call->index));
    if (err < 0) goto error;

    err = at_tok_nextbool(&line, &(p_call->isMT));
    if (err < 0) goto error;

    err = at_tok_nextint(&line, &state);
    if (err < 0) goto error;

    err = clccStateToRILState(state, &(p_call->state));
    if (err < 0) goto error;

    err = at_tok_nextint(&line, &mode);
    if (err < 0) goto error;

    p_call->isVoice = (mode == 0);

    err = at_tok_nextbool(&line, &(p_call->isMpty));
    if (err < 0) goto error;

    if (at_tok_hasmore(&line)) {
        err = at_tok_nextstr(&line, &(p_call->number));

        /* tolerate null here */
        if (err < 0) return 0;

        // Some lame implementations return strings
        // like "NOT AVAILABLE" in the CLCC line
        if (p_call->number != NULL
            && 0 == strspn(p_call->number, "+0123456789")
        ) {
            p_call->number = NULL;
        }

        err = at_tok_nextint(&line, &p_call->toa);
        if (err < 0) goto error;
    }

    p_call->uusInfo = NULL;

    return 0;

error:
    LOGE("invalid CLCC line\n");
    return -1;
}


/** do post-AT+CFUN=1 initialization */
static void onRadioPowerOn()
{
    pollSIMState(NULL);
}

/** do post- SIM ready initialization */
static void onSIMReady()
{
    /**
     *  Wythe: Modify on 2013-04-02 for 4.0 ril
     *  we support AT+CSMS=128 for GSM module and 
     *  AT+CSMS=1 for WCDMA module
     */
     
    at_send_command_singleline("AT+CSMS=128", "+CSMS:", NULL);

    /*
     * Always send SMS messages directly to the TE
     *
     * mode = 1 // discard when link is reserved (link should never be
     *             reserved)
     * mt = 2   // most messages routed to TE
     * bm = 2   // new cell BM's routed to TE
     * ds = 1   // Status reports routed to TE
     * bfr = 1  // flush buffer
     */
     
    at_send_command("AT+CNMI=1,2,2,1,1", NULL);

}

static void requestRadioPower(void *data, size_t datalen, RIL_Token t)
{
    int onOff;

    int err;
    ATResponse *p_response = NULL;

    assert (datalen >= sizeof(int *));
    onOff = ((int *)data)[0];

    if (onOff == 0 && sState != RADIO_STATE_OFF) {
        //just shutdown the RF, keep sim card right
        err = at_send_command("AT+CFUN=4", &p_response);
       if (err < 0 || p_response->success == 0) goto error;
        setRadioState(RADIO_STATE_OFF);
    } else if (onOff > 0 && sState == RADIO_STATE_OFF) {
        err = at_send_command("AT+CFUN=1", &p_response);
        if (err < 0|| p_response->success == 0) {
            // Some stacks return an error when there is no SIM,
            // but they really turn the RF portion on
            // So, if we get an error, let's check to see if it
            // turned on anyway

            if (isRadioOn() != 1) {
                goto error;
            }
        }
        setRadioState(RADIO_STATE_SIM_NOT_READY);
    }

    at_response_free(p_response);
    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
    return;
error:
    LOGE("[GSM]: %s error!\n", __func__);
    at_response_free(p_response);
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
}

static void requestOrSendDataCallList(RIL_Token *t);

static void onDataCallListChanged(void *param)
{
    requestOrSendDataCallList(NULL);
}

static void requestDataCallList(void *data, size_t datalen, RIL_Token t)
{
    requestOrSendDataCallList(&t);
}

static void requestOrSendDataCallList(RIL_Token *t)
{
    ATResponse *p_response;
    ATLine *p_cur;
    int err;
    int n = 0;
    char *out;

    err = at_send_command_multiline ("AT+CGACT?", "+CGACT:", &p_response);
    if (err != 0 || p_response->success == 0) {
        if (t != NULL)
            RIL_onRequestComplete(*t, RIL_E_GENERIC_FAILURE, NULL, 0);
        else
            RIL_onUnsolicitedResponse(RIL_UNSOL_DATA_CALL_LIST_CHANGED,
                                      NULL, 0);
        return;
    }

    for (p_cur = p_response->p_intermediates; p_cur != NULL;
         p_cur = p_cur->p_next)
        n++;

    RIL_Data_Call_Response_v6 *responses =
        alloca(n * sizeof(RIL_Data_Call_Response_v6));

    int i;
    for (i = 0; i < n; i++) {
        responses[i].status = -1;
        responses[i].suggestedRetryTime = -1;
        responses[i].cid = -1;
        responses[i].active = -1;
        responses[i].type = (char *)"";
        responses[i].ifname = (char *)PPP_TTY_PATH;
        responses[i].addresses = (char *)"";
        responses[i].dnses = (char *)"";
        responses[i].gateways = (char *)"";
    }

    RIL_Data_Call_Response_v6 *response = responses;
    for (p_cur = p_response->p_intermediates; p_cur != NULL;
         p_cur = p_cur->p_next) {
        char *line = p_cur->line;

        err = at_tok_start(&line);
        if (err < 0)
            goto error;

        err = at_tok_nextint(&line, &response->cid);
        if (err < 0)
            goto error;

        err = at_tok_nextint(&line, &response->active);
        if (err < 0)
            goto error;

        response++;
    }

    at_response_free(p_response);

    err = at_send_command_multiline ("AT+CGDCONT?", "+CGDCONT:", &p_response);
    if (err != 0 || p_response->success == 0) {
        if (t != NULL)
            RIL_onRequestComplete(*t, RIL_E_GENERIC_FAILURE, NULL, 0);
        else
            RIL_onUnsolicitedResponse(RIL_UNSOL_DATA_CALL_LIST_CHANGED,
                                      NULL, 0);
        return;
    }

    for (p_cur = p_response->p_intermediates; p_cur != NULL;
         p_cur = p_cur->p_next) {
        char *line = p_cur->line;
        int cid;

        err = at_tok_start(&line);
        if (err < 0)
            goto error;

        err = at_tok_nextint(&line, &cid);
        if (err < 0)
            goto error;

        for (i = 0; i < n; i++) {
            if (responses[i].cid == cid)
                break;
        }

        if (i >= n) {
            /* details for a context we didn't hear about in the last request */
            continue;
        }

        // Assume no error
        responses[i].status = 0;

        // type
        err = at_tok_nextstr(&line, &out);
        if (err < 0)
            goto error;
        responses[i].type = alloca(strlen(out) + 1);
        strcpy(responses[i].type, out);

        // APN ignored for v5
        err = at_tok_nextstr(&line, &out);
        if (err < 0)
            goto error;

        responses[i].ifname = alloca(strlen(PPP_TTY_PATH) + 1);
        strcpy(responses[i].ifname, PPP_TTY_PATH);

        err = at_tok_nextstr(&line, &out);
        if (err < 0)
            goto error;

        responses[i].addresses = alloca(strlen(out) + 1);
        strcpy(responses[i].addresses, out);

        {
            char  propValue[PROP_VALUE_MAX];

            if (__system_property_get("ro.kernel.qemu", propValue) != 0) {
                /* We are in the emulator - the dns servers are listed
                 * by the following system properties, setup in
                 * /system/etc/init.goldfish.sh:
                 *  - net.eth0.dns1
                 *  - net.eth0.dns2
                 *  - net.eth0.dns3
                 *  - net.eth0.dns4
                 */
                const int   dnslist_sz = 128;
                char*       dnslist = alloca(dnslist_sz);
                const char* separator = "";
                int         nn;

                dnslist[0] = 0;
                for (nn = 1; nn <= 4; nn++) {
                    /* Probe net.eth0.dns<n> */
                    char  propName[PROP_NAME_MAX];
                    snprintf(propName, sizeof propName, "net.eth0.dns%d", nn);

                    /* Ignore if undefined */
                    if (__system_property_get(propName, propValue) == 0) {
                        continue;
                    }

                    /* Append the DNS IP address */
                    strlcat(dnslist, separator, dnslist_sz);
                    strlcat(dnslist, propValue, dnslist_sz);
                    separator = " ";
                }
                responses[i].dnses = dnslist;

                /* There is only on gateway in the emulator */
                responses[i].gateways = "10.0.2.2";
            }
            else {
                /* I don't know where we are, so use the public Google DNS
                 * servers by default and no gateway.
                 */
                responses[i].dnses = "8.8.8.8 8.8.4.4";
                responses[i].gateways = "";
            }
        }
    }

    at_response_free(p_response);

    if (t != NULL)
        RIL_onRequestComplete(*t, RIL_E_SUCCESS, responses,
                              n * sizeof(RIL_Data_Call_Response_v6));
    else
        RIL_onUnsolicitedResponse(RIL_UNSOL_DATA_CALL_LIST_CHANGED,
                                  responses,
                                  n * sizeof(RIL_Data_Call_Response_v6));

    return;

error:
    LOGE("[GSM]: %s error!\n", __func__);
    if (t != NULL)
        RIL_onRequestComplete(*t, RIL_E_GENERIC_FAILURE, NULL, 0);
    else
        RIL_onUnsolicitedResponse(RIL_UNSOL_DATA_CALL_LIST_CHANGED,
                                  NULL, 0);

    at_response_free(p_response);
}

static void requestQueryNetworkSelectionMode(
                void *data, size_t datalen, RIL_Token t)
{
    int err;
    ATResponse *p_response = NULL;
    int response = 0;
    char *line;

    err = at_send_command_singleline("AT+COPS?", "+COPS:", &p_response);

    if (err < 0 || p_response->success == 0) {
        goto error;
    }

    line = p_response->p_intermediates->line;

    err = at_tok_start(&line);

    if (err < 0) {
        goto error;
    }

    err = at_tok_nextint(&line, &response);

    if (err < 0) {
        goto error;
    }

    RIL_onRequestComplete(t, RIL_E_SUCCESS, &response, sizeof(int));
    at_response_free(p_response);
    return;
error:
    LOGE("[GSM]: %s error!\n", __func__);
    at_response_free(p_response);
    LOGE("requestQueryNetworkSelectionMode must never return error when radio is on");
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
}

static void sendCallStateChanged(void *param)
{
    RIL_onUnsolicitedResponse (
        RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED,
        NULL, 0);
}

static void requestGetCurrentCalls(void *data, size_t datalen, RIL_Token t)
{
    int err;
    ATResponse *p_response;
    ATLine *p_cur;
    int countCalls;
    int countValidCalls;
    RIL_Call *p_calls;
    RIL_Call **pp_calls;
    int i;
    int needRepoll = 0;

    err = at_send_command_multiline ("AT+CLCC", "+CLCC:", &p_response);

    if (err != 0 || p_response->success == 0) {
        LOGE("[GSM]: %s response generic failure\n", __func__);
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
        return;
    }

    /* count the calls */
    for (countCalls = 0, p_cur = p_response->p_intermediates
            ; p_cur != NULL
            ; p_cur = p_cur->p_next
    ) {
        countCalls++;
    }

    /* yes, there's an array of pointers and then an array of structures */

    pp_calls = (RIL_Call **)alloca(countCalls * sizeof(RIL_Call *));
    p_calls = (RIL_Call *)alloca(countCalls * sizeof(RIL_Call));
    memset (p_calls, 0, countCalls * sizeof(RIL_Call));

    /* init the pointer array */
    for(i = 0; i < countCalls ; i++) {
        pp_calls[i] = &(p_calls[i]);
    }

    for (countValidCalls = 0, p_cur = p_response->p_intermediates
            ; p_cur != NULL
            ; p_cur = p_cur->p_next
    ) {
        err = callFromCLCCLine(p_cur->line, p_calls + countValidCalls);

        if (err != 0) {
            continue;
        }

        if (p_calls[countValidCalls].state != RIL_CALL_ACTIVE
            && p_calls[countValidCalls].state != RIL_CALL_HOLDING
        ) {
            needRepoll = 1;
        }
		if(p_calls[countValidCalls].isVoice)
        countValidCalls++;
    }

    LOGI("Calls=%d,Valid=%d\n",countCalls,countValidCalls);
    
    RIL_onRequestComplete(t, RIL_E_SUCCESS, pp_calls,
            countValidCalls * sizeof (RIL_Call *));

    at_response_free(p_response);

#ifdef POLL_CALL_STATE
    if (countValidCalls) {  // We don't seem to get a "NO CARRIER" message from
                            // smd, so we're forced to poll until the call ends.
#else
    if (needRepoll) {
#endif
        RIL_requestTimedCallback (sendCallStateChanged, NULL, &TIMEVAL_CALLSTATEPOLL);
    }

    return;
error:
    LOGD("[GSM]:%s error!\n", __func__);
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    at_response_free(p_response);
}

static void requestDial(void *data, size_t datalen, RIL_Token t)
{
    RIL_Dial *p_dial;
    char *cmd;
    const char *clir;
    int ret;

    p_dial = (RIL_Dial *)data;

    switch (p_dial->clir) {
        case 1: clir = "I"; break;  /*invocation*/
        case 2: clir = "i"; break;  /*suppression*/
        default:
        case 0: clir = ""; break;   /*subscription default*/
    }

    asprintf(&cmd, "ATD%s%s;", p_dial->address, clir);

    ret = at_send_command(cmd, NULL);

    free(cmd);

    /* success or failure is ignored by the upper layer here.
       it will call GET_CURRENT_CALLS and determine success that way */
    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
}

static void requestWriteSmsToSim(void *data, size_t datalen, RIL_Token t)
{
    RIL_SMS_WriteArgs *p_args;
    char *cmd;
    int length;
    int err;
    ATResponse *p_response = NULL;

    p_args = (RIL_SMS_WriteArgs *)data;

    length = strlen(p_args->pdu)/2;
    asprintf(&cmd, "AT+CMGW=%d,%d", length, p_args->status);

    err = at_send_command_sms(cmd, p_args->pdu, "+CMGW:", &p_response);

    if (err != 0 || p_response->success == 0) goto error;

    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
    at_response_free(p_response);

    return;
error:
    LOGE("[GSM]: %s error!\n", __func__);
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    at_response_free(p_response);
}

static void requestHangup(void *data, size_t datalen, RIL_Token t)
{
    int *p_line;

    int ret;
    char *cmd;

    p_line = (int *)data;

    // 3GPP 22.030 6.5.5
    // "Releases a specific active call X"
    asprintf(&cmd, "AT+CHLD=1%d", p_line[0]);

    ret = at_send_command(cmd, NULL);

    free(cmd);

    /* success or failure is ignored by the upper layer here.
       it will call GET_CURRENT_CALLS and determine success that way */
    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
}

static void requestSignalStrength(void *data, size_t datalen, RIL_Token t)
{
    ATResponse *p_response = NULL;
    int err;
    int response[2];
    char *line;

    err = at_send_command_singleline("AT+CSQ", "+CSQ:", &p_response);

    if (err < 0 || p_response->success == 0) {
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
        goto error;
    }

    line = p_response->p_intermediates->line;

    err = at_tok_start(&line);
    if (err < 0) goto error;

    err = at_tok_nextint(&line, &(response[0]));
    if (err < 0) goto error;

    err = at_tok_nextint(&line, &(response[1]));
    if (err < 0) goto error;

#if 1 //quectel
{
    RIL_SignalStrength_v6 signalStrength;
    memset(&signalStrength, 0, sizeof(RIL_SignalStrength_v6));
    /**
     *  In Android Jelly Bean, the invalid value for
     *  LET signalStrength should be 99 depending on the SignalStrength.java.
     */
#if ((PLATFORM_VERSION >= 420) || ((PLATFORM_VERSION < 100) && (PLATFORM_VERSION >= 42)))
    signalStrength.LTE_SignalStrength.signalStrength = 99;
    signalStrength.LTE_SignalStrength.rsrp = 0x7FFFFFFF;
    signalStrength.LTE_SignalStrength.rsrq = 0x7FFFFFFF;
    signalStrength.LTE_SignalStrength.rssnr = 0x7FFFFFFF;
    signalStrength.LTE_SignalStrength.cqi = 0x7FFFFFFF;
#else
    signalStrength.LTE_SignalStrength.signalStrength = -1;
    signalStrength.LTE_SignalStrength.rsrp = -1;
    signalStrength.LTE_SignalStrength.rsrq = -1;
    signalStrength.LTE_SignalStrength.rssnr = -1;
    signalStrength.LTE_SignalStrength.cqi = -1;
#endif	
    signalStrength.GW_SignalStrength.signalStrength = response[0];
    signalStrength.GW_SignalStrength.bitErrorRate = response[1];
    RIL_onRequestComplete(t, RIL_E_SUCCESS, &signalStrength, sizeof(RIL_SignalStrength_v6));
}
#else
    RIL_onRequestComplete(t, RIL_E_SUCCESS, response, sizeof(response));
#endif
    at_response_free(p_response);
    return;

error:
    LOGE("requestSignalStrength must never return an error when radio is on");
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    at_response_free(p_response);
}

static void requestRegistrationState(int request, void *data,
                                        size_t datalen, RIL_Token t)
{
    int err;
    int response[4];
    char * responseStr[4];
    ATResponse *p_response = NULL;
    const char *cmd;
    const char *prefix;
    char *line, *p;
    int commas;
    int skip;
    int count = 3;


    if (request == RIL_REQUEST_VOICE_REGISTRATION_STATE) {
        cmd = "AT+CREG?";
        prefix = "+CREG:";
    } else if (request == RIL_REQUEST_DATA_REGISTRATION_STATE) {
        cmd = "AT+CGREG?";
        prefix = "+CGREG:";
    } else {
        assert(0);
        goto error;
    }

    err = at_send_command_singleline(cmd, prefix, &p_response);

    if (err != 0) goto error;

    line = p_response->p_intermediates->line;

    err = at_tok_start(&line);
    if (err < 0) goto error;

    /* Ok you have to be careful here
     * The solicited version of the CREG response is
     * +CREG: n, stat, [lac, cid]
     * and the unsolicited version is
     * +CREG: stat, [lac, cid]
     * The <n> parameter is basically "is unsolicited creg on?"
     * which it should always be
     *
     * Now we should normally get the solicited version here,
     * but the unsolicited version could have snuck in
     * so we have to handle both
     *
     * Also since the LAC and CID are only reported when registered,
     * we can have 1, 2, 3, or 4 arguments here
     *
     * finally, a +CGREG: answer may have a fifth value that corresponds
     * to the network type, as in;
     *
     *   +CGREG: n, stat [,lac, cid [,networkType]]
     */

    /* count number of commas */
    commas = 0;
    for (p = line ; *p != '\0' ;p++) {
        if (*p == ',') commas++;
    }

    switch (commas) {
        case 0: /* +CREG: <stat> */
            err = at_tok_nextint(&line, &response[0]);
            if (err < 0) goto error;
            response[1] = -1;
            response[2] = -1;
        break;

        case 1: /* +CREG: <n>, <stat> */
            err = at_tok_nextint(&line, &skip);
            if (err < 0) goto error;
            err = at_tok_nextint(&line, &response[0]);
            if (err < 0) goto error;
            response[1] = -1;
            response[2] = -1;
            if (err < 0) goto error;
        break;

        case 2: /* +CREG: <stat>, <lac>, <cid> */
            err = at_tok_nextint(&line, &response[0]);
            if (err < 0) goto error;
            err = at_tok_nexthexint(&line, &response[1]);
            if (err < 0) goto error;
            err = at_tok_nexthexint(&line, &response[2]);
            if (err < 0) goto error;
        break;
        case 3: /* +CREG: <n>, <stat>, <lac>, <cid> */
            err = at_tok_nextint(&line, &skip);
            if (err < 0) goto error;
            err = at_tok_nextint(&line, &response[0]);
            if (err < 0) goto error;
            err = at_tok_nexthexint(&line, &response[1]);
            if (err < 0) goto error;
            err = at_tok_nexthexint(&line, &response[2]);
            if (err < 0) goto error;
        break;
        /* special case for CGREG, there is a fourth parameter
         * that is the network type (unknown/gprs/edge/umts)
         */
        case 4: /* +CGREG: <n>, <stat>, <lac>, <cid>, <networkType> */
            err = at_tok_nextint(&line, &skip);
            if (err < 0) goto error;
            err = at_tok_nextint(&line, &response[0]);
            if (err < 0) goto error;
            err = at_tok_nexthexint(&line, &response[1]);
            if (err < 0) goto error;
            err = at_tok_nexthexint(&line, &response[2]);
            if (err < 0) goto error;
            err = at_tok_nexthexint(&line, &response[3]);
            if (err < 0) goto error;
            count = 4;
        break;
        default:
            goto error;
    }

    asprintf(&responseStr[0], "%d", response[0]);
    asprintf(&responseStr[1], "%x", response[1]);
    asprintf(&responseStr[2], "%x", response[2]);

#if 1
    if (request == RIL_REQUEST_DATA_REGISTRATION_STATE) {
    count = 4;
    response[3] = 1;
    asprintf(&responseStr[3], "%d", response[3]);
    LOGI("\n\n\nJOE\n\n\n");
    }
#else
    if (count > 3)
        asprintf(&responseStr[3], "%d", response[3]);
#endif
    RIL_onRequestComplete(t, RIL_E_SUCCESS, responseStr, count*sizeof(char*));
    at_response_free(p_response);

    return;
error:
    LOGE("requestRegistrationState must never return an error when radio is on");
    LOGE("[GSM]: %s error!\n", __func__);
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    at_response_free(p_response);
}

static void requestOperator(void *data, size_t datalen, RIL_Token t)
{
    int err;
    int i;
    int skip;
    ATLine *p_cur;
    char *response[3];

    memset(response, 0, sizeof(response));

    ATResponse *p_response = NULL;

    err = at_send_command_multiline(
        "AT+COPS=3,0;+COPS?;+COPS=3,1;+COPS?;+COPS=3,2;+COPS?",
        "+COPS:", &p_response);

    /* we expect 3 lines here:
     * +COPS: 0,0,"T - Mobile"
     * +COPS: 0,1,"TMO"
     * +COPS: 0,2,"310170"
     */

    if (err != 0) goto error;

    for (i = 0, p_cur = p_response->p_intermediates
            ; p_cur != NULL
            ; p_cur = p_cur->p_next, i++
    ) {
        char *line = p_cur->line;

        err = at_tok_start(&line);
        if (err < 0) goto error;

        err = at_tok_nextint(&line, &skip);
        if (err < 0) goto error;

        // If we're unregistered, we may just get
        // a "+COPS: 0" response
        if (!at_tok_hasmore(&line)) {
            response[i] = NULL;
            continue;
        }

        err = at_tok_nextint(&line, &skip);
        if (err < 0) goto error;

        // a "+COPS: 0, n" response is also possible
        if (!at_tok_hasmore(&line)) {
            response[i] = NULL;
            continue;
        }

        err = at_tok_nextstr(&line, &(response[i]));
        if (err < 0) goto error;
    }

    if (i != 3) {
        /* expect 3 lines exactly */
        goto error;
    }

    RIL_onRequestComplete(t, RIL_E_SUCCESS, response, sizeof(response));
    at_response_free(p_response);

    return;
error:
    LOGE("requestOperator must not return error when radio is on");
    LOGE("[GSM]: %s error!\n", __func__);
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    at_response_free(p_response);
}

static void requestSendSMS(void *data, size_t datalen, RIL_Token t)
{
    int err;
    const char *smsc;
    const char *pdu;
    int tpLayerLength;
    char *cmd1, *cmd2;
    RIL_SMS_Response response;
    ATResponse *p_response = NULL;

    smsc = ((const char **)data)[0];
    pdu = ((const char **)data)[1];

    tpLayerLength = strlen(pdu)/2;

    // "NULL for default SMSC"
    if (smsc == NULL) {
        smsc= "00";
    }

    asprintf(&cmd1, "AT+CMGS=%d", tpLayerLength);
    asprintf(&cmd2, "%s%s", smsc, pdu);

    err = at_send_command_sms(cmd1, cmd2, "+CMGS:", &p_response);

    if (err != 0 || p_response->success == 0) goto error;

    memset(&response, 0, sizeof(response));

    /* FIXME fill in messageRef and ackPDU */

    RIL_onRequestComplete(t, RIL_E_SUCCESS, &response, sizeof(response));
    at_response_free(p_response);

    return;
error:
    LOGE("[GSM]: %s error!\n", __func__);
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    at_response_free(p_response);
}

/* wait for a propertyvalue change */
static int wait_for_property(const char *name, const char *desired_value, int maxwait)
{
    char value[PROPERTY_VALUE_MAX] = {'\0'};
    int maxnaps = maxwait / 1;

    if (maxnaps < 1) {
        maxnaps = 1;
    }

    while (maxnaps-- > 0) {
        usleep(1000000);
        if (property_get(name, value, NULL)) {
			LOGI("[GSM]:%s return 0!\n", name);
            if (desired_value == NULL ||
                    strcmp(value, desired_value) == 0) {
				LOGI("[GSM]:%s return 0!\n", name);
                return 0;
            }
        }
    }
	LOGI("[wythe]:%s return -1!\n", name);
    return -1; /* failure */
}


static int kill_PPP(const char * cid)
{
    int err;
    int fd;
    int i=0;
    char pppd_pid_str[PROPERTY_VALUE_MAX];
    int pppd_pid = 0;

    LOGD("kill_PPP");

    char *cmd = NULL;

#if 0
    while ((fd = open("/sys/class/net/ppp0/ifindex",O_RDONLY)) > 0)
    {
        if(i%5 == 0)
            system("killall pppd");
        if(i>25)
            goto error;
        i++;
        close(fd);
        sleep(2);
    }
#else
    property_get("net.gprs.ppp-pid", pppd_pid_str, "-1");
    pppd_pid = atoi(pppd_pid_str);
    if (pppd_pid > 0) {
        LOGD("terminate pppd_pid %d, and wait ...", pppd_pid);
        if (kill(pppd_pid, SIGTERM) == 0) {
			sleep(3);
            //if (wait_for_property("net.gprs.ppp-pid", "-1", 5) < 0) {
            //    kill(pppd_pid, SIGKILL);
            //    wait_for_property("net.gprs.ppp-pid", "-1", 3);
            //}
        }
    }
    property_set("net.gprs.ppp-pid", "-1");

    // Requires root access...
    property_set("ctl.stop", "pppd_gprs");
    if (wait_for_property("init.svc.pppd_gprs", "stopped", 10) < 0) {
        goto error;
    }
#endif

    LOGD("killall pppd finished");

    asprintf(&cmd, "AT+CGACT=0,%s", cid);

    err = at_send_command(cmd, NULL);
    if (err != 0)
        goto error;

    at_send_command("ATH",NULL);
    return 0;

error:
    return -1;
}

static void requestSetupDataCall(void *data, size_t datalen, RIL_Token t)
{
#if 0
    const char *apn;
	const char *user = NULL;
	const char *pass = NULL;
	const char *pdp_type = NULL;
	char *cmd;
	char userpass[512];
	int err;
	int fd, pppstatus,i;
	FILE *pppconfig;
	size_t cur = 0;
	ssize_t written, rlen;
	char status[32] = {0};
	char *buffer;
	long buffSize, len;
	int retry = 10;
    ATResponse *p_response = NULL;
	
	int n = 1;
	RIL_Data_Call_Response_v6 responses;
	char ppp_dnses[(PROPERTY_VALUE_MAX * 2) + 3] = {'\0'};
	char ppp_local_ip[PROPERTY_VALUE_MAX] = {'\0'};
	char ppp_dns1[PROPERTY_VALUE_MAX] = {'\0'};
	char ppp_dns2[PROPERTY_VALUE_MAX] = {'\0'};
	char ppp_gw[PROPERTY_VALUE_MAX] = {'\0'};
	
	apn = ((const char **)data)[2];
	user = ((char **)data)[3];
	if(user != NULL)
	{
		if (strlen(user)<2)
			user = "dummy";
	} else
		user = "dummy";

	pass = ((char **)data)[4];
	if(pass != NULL)
	{
		if (strlen(pass)<2)
			pass = "dummy";
	} else
		pass = "dummy";

	LOGD("requesting data connection to APN '%s'\n", apn);

	//Make sure there is no existing connection or pppd instance running
	if(kill_PPP("1") < 0) {
		LOGE("killConn Error!\n");
		goto error;
	}

        if (datalen > 6 * sizeof(char *)) {
            pdp_type = ((const char **)data)[6];
        } else {
            pdp_type = "IP";
        }

    asprintf(&cmd, "AT+CGDCONT=1,\"%s\",\"%s\"", pdp_type, apn);
     
    //FIXME check for error here
    err = at_send_command(cmd, NULL);
    free(cmd);

	sleep(2); //Wait for the modem to finish

	//set up the pap/chap secrets file
	sprintf(userpass, "%s * %s", user, pass);
	
	/* start the gprs pppd */
	property_set("ctl.start", "pppd_gprs");

	sleep(5); // Allow time for ip-up to complete

	if (wait_for_property("net.ppp0.local-ip", NULL, 10) < 0) {
		LOGE("wait_for_property timeout waiting net.ppp0.local-ip - giving up!\n");
		kill_PPP(1);
		asprintf(&cmd, "/system/bin/pppd connect 'system/bin/chat -esvf /system/etc/ppp/peers/chat-gsm-m35-connect' user %s password %s /dev/ttyUSB3 115200 nocrtscts modem novj noipdefault nobsdcomp usepeerdns defaultroute noauth debug nodetach dump &", (user == NULL)?"any":user, (pass == NULL)?"any":pass);
		err = system(cmd);
		LOGD("[wythe]Launch: %s and ret is %d!\n", cmd, err);
		free(cmd);
	}

	if(wait_for_property("net.ppp0.local-ip", NULL, 10) < 0)
	{
		LOGE("[wythe]System exec pppd timeout!\n");
		goto error;
	}

	property_get("net.ppp0.local-ip", ppp_local_ip, NULL);
	property_get("net.ppp0.dns1", ppp_dns1, NULL);
	property_get("net.ppp0.dns2", ppp_dns2, NULL);
	property_get("net.ppp0.gw", ppp_gw, NULL);
	sprintf(ppp_dnses, "%s %s", ppp_dns1, ppp_dns2);

	LOGI("Got net.ppp0.local-ip: %s\n", ppp_local_ip);

	responses.status = 0;
	responses.suggestedRetryTime = -1;
	responses.cid = 1;
	responses.active = 2;
	responses.type = (char*)"PPP";
	responses.ifname = (char*)PPP_TTY_PATH;
	responses.addresses = ppp_local_ip;
	responses.dnses = ppp_dnses;
	responses.gateways = ppp_gw;
	
	RIL_onRequestComplete(t, RIL_E_SUCCESS, &responses, sizeof(RIL_Data_Call_Response_v6));
    at_response_free(p_response);
	return;

error:
	LOGE("Unable to setup PDP in %s\n", __func__);
	RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    at_response_free(p_response);
    
#else 

    const char *apn;
#ifdef PPP_USE_USER_PASS 
	const char *user = NULL;
	const char *pass = NULL;
#endif    
	const char *pdp_type = NULL;
	char *cmd;
	//char userpass[512];
	int err;
	int fd, pppstatus,i;
	FILE *pppconfig;
	size_t cur = 0;
	ssize_t written, rlen;
	char status[32] = {0};
	char *buffer;
	long buffSize, len;
	int retry = 10;
    ATResponse *p_response = NULL;
    int iRetry = PPP_START_RETRY_TIME;
	
	int n = 1;
	RIL_Data_Call_Response_v6 responses;
	char ppp_dnses[(PROPERTY_VALUE_MAX * 2) + 3] = {'\0'};
	char ppp_local_ip[PROPERTY_VALUE_MAX] = {'\0'};
	char ppp_dns1[PROPERTY_VALUE_MAX] = {'\0'};
	char ppp_dns2[PROPERTY_VALUE_MAX] = {'\0'};
	char ppp_gw[PROPERTY_VALUE_MAX] = {'\0'};
    char exit_code[PROPERTY_VALUE_MAX] = {'\0'};
	
	apn = ((const char **)data)[2];

#ifdef PPP_USE_USER_PASS    
	user = ((char **)data)[3];
	if(user != NULL)
	{
		if (strlen(user)<2){
            LOGD("[%s]: length of user name is too short\r\n", __func__);
			user = "dummy";
		}else{
            LOGD("[%s]: Input user name is %s\r\n", user);
        }
	} else{
	    LOGD("[%s]: user name is null\r\n", __func__);
		user = "dummy";
	}

	pass = ((char **)data)[4];
	if(pass != NULL)
	{
		if (strlen(pass)<2){
            LOGD("[%s]: length of password is too short\r\n", __func__);
			pass = "dummy";
		}else{
		    LOGD("[%s]: Input password is %s\r\n", pass);
		}
	} else{
	    LOGD("[%s]: pass word is null\r\n", __func__);
		pass = "dummy";
	}
#endif    

	LOGD("requesting data connection to APN '%s'\n", apn);

	//Make sure there is no existing connection or pppd instance running
	if(kill_PPP("1") < 0) {
		LOGE("killConn Error!\n");
		goto error;
	}

        if (datalen > 6 * sizeof(char *)) {
            pdp_type = ((const char **)data)[6];
        } else {
            pdp_type = "IP";
        }

    asprintf(&cmd, "AT+CGDCONT=1,\"%s\",\"%s\"", pdp_type, apn);
     
    //FIXME check for error here
    err = at_send_command(cmd, NULL);
    free(cmd);

	sleep(2); //Wait for the modem to finish

	//set up the pap/chap secrets file
	//sprintf(userpass, "%s * %s", user, pass);
	
	/* start the gprs pppd */
    property_set(PPPD_EXIT_CODE, "");
    property_set("net.ppp0.local-ip", "");
	property_set("ctl.start", "pppd_gprs");

	sleep(5); // sleep to wati pppd finish

    //  retry 5 time to wait the ip from ppp link
    //  it will be failed to get the ip or the pppd exit 
    while(iRetry > 0)
    {
        property_get(PPPD_EXIT_CODE, exit_code, "");
        
        if(strcmp(exit_code, "") != 0) {
	        LOGE("PPPD exit with code %s", exit_code);
	        //iRetry = 0;
	        //break;
        }
        
        if (wait_for_property("net.ppp0.local-ip", NULL, 10) < 0) {
		    LOGE("[%s]: wait for IP from ppp link at %d\n", __func__, iRetry);
	    }
        else
        {
            LOGI("[%s]: got IP from ppp link\r\n", __func__);
            break;
        }
        iRetry--;
    }

    if(iRetry <= 0)
    {
        LOGE("[%s]: fail to get IP\r\n", __func__);
        goto error;
    }

	property_get("net.ppp0.local-ip", ppp_local_ip, NULL);
	property_get("net.ppp0.dns1", ppp_dns1, NULL);
	property_get("net.ppp0.dns2", ppp_dns2, NULL);
	property_get("net.ppp0.gw", ppp_gw, NULL);
	sprintf(ppp_dnses, "%s %s", ppp_dns1, ppp_dns2);

	LOGI("Got net.ppp0.local-ip: %s\n", ppp_local_ip);

	responses.status = 0;
	responses.suggestedRetryTime = -1;
	responses.cid = 1;
	responses.active = 2;
	responses.type = (char*)"PPP";
	responses.ifname = (char*)PPP_TTY_PATH;
	responses.addresses = ppp_local_ip;
	responses.dnses = ppp_dnses;
	responses.gateways = ppp_gw;
	
	RIL_onRequestComplete(t, RIL_E_SUCCESS, &responses, sizeof(RIL_Data_Call_Response_v6));
    at_response_free(p_response);
	return;

error:
	LOGE("Unable to setup PDP in %s\n", __func__);
	RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    at_response_free(p_response);
#endif    
}

static void requestSMSAcknowledge(void *data, size_t datalen, RIL_Token t)
{
    int ackSuccess;
    int err;

    ackSuccess = ((int *)data)[0];

    if (ackSuccess == 1) {
        err = at_send_command("AT+CNMA=1", NULL);
    } else if (ackSuccess == 0)  {
        err = at_send_command("AT+CNMA=2", NULL);
    } else {
        LOGE("unsupported arg to RIL_REQUEST_SMS_ACKNOWLEDGE\n");
        goto error;
    }

    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
error:
    LOGE("[GSM]: %s error!\n", __func__);
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);

}

static void  requestSIM_IO(void *data, size_t datalen, RIL_Token t)
{
    ATResponse *p_response = NULL;
    RIL_SIM_IO_Response sr;
    int err;
    char *cmd = NULL;
    RIL_SIM_IO_v6 *p_args;
    char *line;

    memset(&sr, 0, sizeof(sr));

    p_args = (RIL_SIM_IO_v6 *)data;

    /* FIXME handle pin2 */

    if (p_args->data == NULL) {
        asprintf(&cmd, "AT+CRSM=%d,%d,%d,%d,%d",
                    p_args->command, p_args->fileid,
                    p_args->p1, p_args->p2, p_args->p3);
    } else {
        asprintf(&cmd, "AT+CRSM=%d,%d,%d,%d,%d,%s",
                    p_args->command, p_args->fileid,
                    p_args->p1, p_args->p2, p_args->p3, p_args->data);
    }

    err = at_send_command_singleline(cmd, "+CRSM:", &p_response);

    if (err < 0 || p_response->success == 0) {
        goto error;
    }

    line = p_response->p_intermediates->line;

    err = at_tok_start(&line);
    if (err < 0) goto error;

    err = at_tok_nextint(&line, &(sr.sw1));
    if (err < 0) goto error;

    err = at_tok_nextint(&line, &(sr.sw2));
    if (err < 0) goto error;

    if (at_tok_hasmore(&line)) {
        err = at_tok_nextstr(&line, &(sr.simResponse));
        if (err < 0) goto error;
    }

    RIL_onRequestComplete(t, RIL_E_SUCCESS, &sr, sizeof(sr));
    at_response_free(p_response);
    free(cmd);

    return;
error:
    LOGE("[GSM]: %s error!\n", __func__);
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    at_response_free(p_response);
    free(cmd);

}

static void  requestEnterSimPin(void*  data, size_t  datalen, RIL_Token  t)
{
    ATResponse   *p_response = NULL;
    int           err;
    char*         cmd = NULL;
    const char**  strings = (const char**)data;;

    if ( datalen == sizeof(char*) ) {
        asprintf(&cmd, "AT+CPIN=%s", strings[0]);
    } else if ( datalen == 2*sizeof(char*) ) {
        asprintf(&cmd, "AT+CPIN=%s,%s", strings[0], strings[1]);
    } else
        goto error;

    err = at_send_command_singleline(cmd, "+CPIN:", &p_response);
    free(cmd);

    if (err < 0 || p_response->success == 0) {
error:
    LOGE("[GSM]: %s error!\n", __func__);
        RIL_onRequestComplete(t, RIL_E_PASSWORD_INCORRECT, NULL, 0);
    } else {
        RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
    }
    at_response_free(p_response);
}


static void  requestSendUSSD(void *data, size_t datalen, RIL_Token t)
{
    const char *ussdRequest;

    ussdRequest = (char *)(data);


    RIL_onRequestComplete(t, RIL_E_REQUEST_NOT_SUPPORTED, NULL, 0);

// @@@ TODO

}

/**
 *  Wythe:Add on 2013-04-02 for 4.0 ril
 *  Add new function to handle more requests.
 *  Must response to RIL.JAVA.
 */

/*
* Function: requestScreenState
* Purpose : handle the request when screen is on or off.
* Request : RIL_REQUEST_SCREEN_STATE
*/
static void requestScreenState(void *data, size_t datalen, RIL_Token t)
{
	int err, screenState;

	assert (datalen >= sizeof(int *));
	screenState = ((int*)data)[0];

	if(screenState == 1)
	{
		/* Screen on - enable unsolicited notifications */
		
		/* Enable GSM network registration notifications */
		err = at_send_command("AT+CREG=2", NULL);
		if (err != 0) goto error;
		
		/* Enable GPRS network registration notifications */
		err = at_send_command("AT+CGREG=2", NULL);
		if (err != 0) goto error;

		/* Enable unsolicited reports */
		err = at_send_command("AT+QIURC=1", NULL);
		if (err != 0) goto error;
		
		/* Enable GPRS reporting */
		err = at_send_command("AT+CGEREP=1", NULL);
		if (err != 0) goto error;

        /* Maybe other command need add here */
		
	} else if (screenState == 0) {

		/* Screen off - disable all unsolicited notifications */
		err = at_send_command("AT+CREG=0", NULL);
		if (err != 0) goto error;
		err = at_send_command("AT+CGREG=0", NULL);
		if (err != 0) goto error;
		err = at_send_command("AT+QIURC=0", NULL);
		if (err != 0) goto error;
		err = at_send_command("AT+CGEREP=0", NULL);
		if (err != 0) goto error;
        
		/* Maybe other command need add here */
		
	} else {
		/* Not a defined value - error */
		goto error;
	}

	RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
	return;

error:
	LOGE("[Wythe]ERROR: requestScreenState failed");
	RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
}

/*
*   Function: requestBaseBandVersion
*   Purpose : return string of BaseBand version
*   Request : RIL_REQUEST_BASEBAND_VERSION
*/
static void requestBaseBandVersion(void *data, size_t datalen, RIL_Token t)
{
    int err;
    ATResponse *atResponse = NULL;
    char *line;

    err = at_send_command_singleline("AT+CGMR", "\0", &atResponse);

    if(err != 0){
        LOGE("[Wythe]%s() Error Reading Base Band Version!",__func__);
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
        return;
    }

    line = atResponse->p_intermediates->line;

    RIL_onRequestComplete(t, RIL_E_SUCCESS, line, sizeof(char *));
    
    at_response_free(atResponse);
}


/*
*   Function: requestGetIMEISV 
*   Purpose : return the IMEI SV(software version)
*   Request : RIL_REQUEST_GET_IMEISV
*/
static void requestGetIMEISV(RIL_Token t)
{
    RIL_onRequestComplete(t, RIL_E_SUCCESS, (void *)00, sizeof(char *));
}

/**
 *  Function: requestSetPreferredNetworkType
 *  Purpose : Requests to set the preferred network type for searching and registering
 *            (CS/PS domain, RAT, and operation mode).
 *  Request : RIL_REQUEST_SET_PREFERRED_NETWORK_TYPE
 */ 
static void requestSetPreferredNetworkType(void *data, size_t datalen, RIL_Token t)
{
	int rat,err;
	const char* cmd;
	
	assert (datalen >= sizeof(int *));
	rat = ((int *)data)[0];

	/*  specific 	
     * ((int *)data)[0] is == 0 for GSM/WCDMA (WCDMA preferred)
     * ((int *)data)[0] is == 1 for GSM only
     * ((int *)data)[0] is == 2 for WCDMA only
     * ((int *)data)[0] is == 3 for GSM/WCDMA (auto mode, according to PRL)
     * ((int *)data)[0] is == 4 for CDMA and EvDo (auto mode, according to PRL)
     * ((int *)data)[0] is == 5 for CDMA only
     * ((int *)data)[0] is == 6 for EvDo only
     * ((int *)data)[0] is == 7 for GSM/WCDMA, CDMA, and EvDo (auto mode, according to PRL)
	 */
	
	switch (rat) {
		default: 	/* For 2G module, it only work in GSM */
			break; 
	}
    
#if 0
	/* Set mode */
	err = at_send_command(cmd);
	if (err != 0) 
		goto error;
#endif

	/* Trigger autoregister */
	err = at_send_command("AT+COPS=0", NULL);
	if (err != 0) 
		goto error;

	RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, sizeof(int));
	return;
	
error:
	LOGE("ERROR: requestSetPreferredNetworkType() failed\n");
	RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
}

/*
*   Function: requestReportSTKServiceIsRunning
*   Purpose :
*   Request : RIL_REQUEST_REPORT_STK_SERVICE_IS_RUNNING
*/
static void requestReportSTKServiceIsRunning(RIL_Token t)
{
    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
}

/* 
 * Function: requestDeactivateDefaultPDP
 * Purpose :
 * Request : RIL_REQUEST_DEACTIVATE_DATA_CALL
 */ 
static void requestDeactivateDefaultPDP(void *data, size_t datalen, RIL_Token t)
{
	char * cid;

	LOGD("requestDeactivateDefaultPDP()");

	cid = ((char **)data)[0];
	if (kill_PPP(cid) < 0)
		goto error;

	RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
	return;

error:
    LOGE("[GSM]:%s error!\n",__func__);
	RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
}

/**
 * Funtcion:requestLastPDPFailCause
 *
 * Purpose :Requests the failure cause code for the most recently failed PDP
 * context activate.
 *
 * Request : RIL_REQUEST_LAST_CALL_FAIL_CAUSE.
 *
 */
static int s_lastPdpFailCause = PDP_FAIL_ERROR_UNSPECIFIED;
static void requestLastPDPFailCause(RIL_Token t)
{
    RIL_onRequestComplete(t, RIL_E_SUCCESS, &s_lastPdpFailCause, sizeof(int));
} 

static void requestSetMute(void *data, size_t datalen, RIL_Token t)
{
	int err;
    char *cmd = NULL;
    
	assert (datalen >= sizeof(int *));

	/* mute */
    asprintf(&cmd, "AT+CMUT=%d", ((int*)data)[0]);
	err = at_send_command(cmd, NULL);
    
	if (err != 0) 
		goto error;

	RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
	return;

error:
	LOGE("ERROR: requestSetMute failed");
	RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);

}

static void requestGetMute(RIL_Token t)
{
	int err;
	ATResponse *atResponse = NULL;
	int response[1];
	char *line;

	err = at_send_command_singleline("AT+CMUT?", "+CMUT:", &atResponse);

	if (err != 0)
		goto error;

	line = atResponse->p_intermediates->line;

	err = at_tok_start(&line);
	if (err < 0) goto error;

	err = at_tok_nextint(&line, &response[0]);
	if (err < 0) goto error;

	RIL_onRequestComplete(t, RIL_E_SUCCESS, response, sizeof(char*));
	at_response_free(atResponse);

	return;

error:
	LOGE("ERROR: requestGetMute failed");
	RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
	at_response_free(atResponse);
}

/**
 *  Wythe add on 2014-1-26 ->start 
 *  Add handler to deal with SIM lock request.
 */
 
/**
 * RIL_REQUEST_CHANGE_SIM_PIN
 * RIL_REQUEST_CHANGE_SIM_PIN2
*/ 
static void requestChangePassword(void *data, size_t datalen, RIL_Token t,
                           const char *facility, int request)
{
    int err = 0;
    char *oldPassword = NULL;
    char *newPassword = NULL;
    char *cmd = NULL;
    int num_retries = -1;

    if (datalen != 3 * sizeof(char *) || strlen(facility) != 2)
        goto error;


    oldPassword = ((char **) data)[0];
    newPassword = ((char **) data)[1];

    asprintf(&cmd, "AT+CPWD=\"%s\",\"%s\",\"%s\"", facility, oldPassword, newPassword);

#if 0
    err = at_send_command("AT+CPWD=\"%s\",\"%s\",\"%s\"", facility,
                oldPassword, newPassword);
#else
    err = at_send_command(cmd, NULL);
#endif

    if (err != 0)
        goto error;

    num_retries = 3;
    RIL_onRequestComplete(t, RIL_E_SUCCESS, &num_retries, sizeof(int *));

    return;

error:
    if (at_get_cme_error(err) == CME_INCORRECT_PASSWORD) {
        num_retries = 3;
        RIL_onRequestComplete(t, RIL_E_PASSWORD_INCORRECT, &num_retries, sizeof(int *));
    } else
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
}

/**
 * RIL_REQUEST_QUERY_FACILITY_LOCK
 *
 * Query the status of a facility lock state.
 */
static void requestQueryFacilityLock(void *data, size_t datalen, RIL_Token t)
{
    int err, response;
    ATResponse *atResponse = NULL;
    char *line = NULL;
    char *facility_string = NULL;
    char *facility_password = NULL;
    char *facility_class = NULL;
    char *cmd = NULL;

    (void) datalen;

    if (datalen < 3 * sizeof(char **)) {
        LOGE("%s() bad data length!", __func__);
        goto error;
    }

    facility_string = ((char **) data)[0];
    facility_password = ((char **) data)[1];
    facility_class = ((char **) data)[2];

    asprintf(&cmd, "AT+CLCK=\"%s\",2,%s,%s", facility_string, facility_password, facility_class);
    
#if 0
    err = at_send_command_singleline("AT+CLCK=\"%s\",2,%s,%s", "+CLCK:", &atResponse,
            facility_string, facility_password, facility_class);
#else
    err = at_send_command_singleline(cmd, "+CLCK:", &atResponse);
#endif

    if (err < 0 || atResponse->success == 0)
        goto error;

    line = atResponse->p_intermediates->line;

    err = at_tok_start(&line);

    if (err < 0)
        goto error;

    err = at_tok_nextint(&line, &response);

    if (err < 0)
        goto error;

    RIL_onRequestComplete(t, RIL_E_SUCCESS, &response, sizeof(int));
    at_response_free(atResponse);
    return;

error:
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    at_response_free(atResponse);
} 

/**
 * RIL_REQUEST_SET_FACILITY_LOCK
 *
 * Enable/disable one facility lock.
 */
static void requestSetFacilityLock(void *data, size_t datalen, RIL_Token t)
{
    int err;
    char *facility_string = NULL;
    int facility_mode = -1;
    char *facility_mode_str = NULL;
    char *facility_password = NULL;
    char *facility_class = NULL;
    char *cmd = NULL;
    int num_retries = -1;
    RIL_Errno errorril = RIL_E_GENERIC_FAILURE;
    (void) datalen;

    if (datalen < 4 * sizeof(char **)) {
        LOGE("%s() bad data length!", __func__);
        goto exit;
    }

    facility_string = ((char **) data)[0];
    facility_mode_str = ((char **) data)[1];
    facility_password = ((char **) data)[2];
    facility_class = ((char **) data)[3];

    if (*facility_mode_str != '0' && *facility_mode_str != '1') {
        LOGE("%s() bad facility mode!", __func__);
        goto exit;
    }

    facility_mode = atoi(facility_mode_str);

    asprintf(&cmd, "AT+CLCK=\"%s\",%d,\"%s\",%s", facility_string,
            facility_mode, facility_password, facility_class);

    /*
     * Skip adding facility_password to AT command parameters if it is NULL,
     * printing NULL with %s will give string "(null)".
     */

#if 0    
    err = at_send_command("AT+CLCK=\"%s\",%d,\"%s\",%s", facility_string,
            facility_mode, facility_password, facility_class);
#else
    err = at_send_command(cmd, NULL);
#endif
    
    if (err != 0) {
        switch (at_get_cme_error(err)) {
        /* CME ERROR 11: "SIM PIN required" happens when PIN is wrong */
        case CME_SIM_PIN_REQUIRED:
            LOGI("Wrong PIN");
            errorril = RIL_E_PASSWORD_INCORRECT;
            break;
        /*
         * CME ERROR 12: "SIM PUK required" happens when wrong PIN is used
         * 3 times in a row
         */
        case CME_SIM_PUK_REQUIRED:
            LOGI("PIN locked, change PIN with PUK");
            num_retries = 0;/* PUK required */
            errorril = RIL_E_PASSWORD_INCORRECT;
            break;
        /* CME ERROR 16: "Incorrect password" happens when PIN is wrong */
        case CME_INCORRECT_PASSWORD:
            LOGI("Incorrect password, Facility: %s", facility_string);
            errorril = RIL_E_PASSWORD_INCORRECT;
            break;
        /* CME ERROR 17: "SIM PIN2 required" happens when PIN2 is wrong */
        case CME_SIM_PIN2_REQUIRED:
            LOGI("Wrong PIN2");
            errorril = RIL_E_PASSWORD_INCORRECT;
            break;
        /*
         * CME ERROR 18: "SIM PUK2 required" happens when wrong PIN2 is used
         * 3 times in a row
         */
        case CME_SIM_PUK2_REQUIRED:
            LOGI("PIN2 locked, change PIN2 with PUK2");
            num_retries = 0;/* PUK2 required */
            errorril = RIL_E_SIM_PUK2;
            break;
        default: /* some other error */
            num_retries = -1;
            break;
        }
        goto finally;
    }

    errorril = RIL_E_SUCCESS;

finally:
    if (strncmp(facility_string, "SC", 2) == 0)
        num_retries = 1;
    else if  (strncmp(facility_string, "FD", 2) == 0)
        num_retries = 1;
exit:
    RIL_onRequestComplete(t, errorril, &num_retries,  sizeof(int *));
} 

/**
 *  Wythe add on 2014-1-26 ->end 
 *  Add handler to deal with SIM lock request.
 */


/*** Callback methods from the RIL library to us ***/

/**
 * Call from RIL to us to make a RIL_REQUEST
 *
 * Must be completed with a call to RIL_onRequestComplete()
 *
 * RIL_onRequestComplete() may be called from any thread, before or after
 * this function returns.
 *
 * Will always be called from the same thread, so returning here implies
 * that the radio is ready to process another command (whether or not
 * the previous command has completed).
 */
static void
onRequest (int request, void *data, size_t datalen, RIL_Token t)
{
    ATResponse *p_response;
    int err;

    LOGD("onRequest: %s", requestToString(request));

    /**
     *  Wythe: Add on 2013-04-02 for 4.0 RIL
     *  can not operate SIM Card when sim is not ready.
     *  It's just return GENERIC_FAILURE.
     */
    if(RADIO_STATE_SIM_READY != sState
        && (RIL_REQUEST_WRITE_SMS_TO_SIM == request ||
           RIL_REQUEST_DELETE_SMS_ON_SIM == request)){
        LOGD("[GSM]:radio state =%d, onRequest: %s\r\n", sState, requestToString(request));
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
        return;
    }

    /**
     *  Wythe:Add on 2013-04-02 for 4.0 RIL
     *  It's able to get sim status even the radio 
     *  is unavailable.
     */
    if(RADIO_STATE_UNAVAILABLE == sState
        && RIL_REQUEST_GET_SIM_STATUS != request){
        LOGD("[GSM]:radio state =%d, onRequest: %s\r\n", sState, requestToString(request));
        RIL_onRequestComplete(t,RIL_E_RADIO_NOT_AVAILABLE,NULL,0);
        return;
    }

    /**
     *  Wythe:Add on 2013-04-02 for 4.0 RIL
     *  These commands can be executed even the radio is in STATE_OFF
     *  or STATE_SIM_NOT_READY.
     */
    if((RADIO_STATE_OFF == sState || RADIO_STATE_SIM_NOT_READY == sState)
        && !(RIL_REQUEST_RADIO_POWER == request ||
             RIL_REQUEST_GET_SIM_STATUS == request ||
             RIL_REQUEST_STK_GET_PROFILE == request ||
             RIL_REQUEST_STK_SET_PROFILE == request ||
             RIL_REQUEST_REPORT_STK_SERVICE_IS_RUNNING == request ||
             RIL_REQUEST_GET_IMEI == request ||
             RIL_REQUEST_GET_IMEISV ==request ||
             RIL_REQUEST_BASEBAND_VERSION ==request ||
             RIL_REQUEST_SCREEN_STATE == request)){
        LOGD("[GSM]:radio state =%d, onRequest: %s\r\n", sState, requestToString(request));     
        RIL_onRequestComplete(t, RIL_E_RADIO_NOT_AVAILABLE, NULL, 0);
        return;
    }

    /**
     *  Wythe: Add on 2013-04-02 for 4.0 ril
     *  When SIM Card is locked or absent, it's not able to
     *  execute these commands. We just return GENERIC_FAILURE for these requests.
     */
    if(RADIO_STATE_SIM_LOCKED_OR_ABSENT == sState
        && !(RIL_REQUEST_ENTER_SIM_PIN == request ||
             RIL_REQUEST_ENTER_SIM_PIN2 == request ||
             RIL_REQUEST_ENTER_SIM_PUK == request ||
             RIL_REQUEST_ENTER_SIM_PUK2 == request ||
             RIL_REQUEST_ENTER_NETWORK_DEPERSONALIZATION ==request ||
             RIL_REQUEST_GET_SIM_STATUS == request ||
             RIL_REQUEST_RADIO_POWER == request ||
             RIL_REQUEST_GET_IMEISV == request ||
             RIL_REQUEST_GET_IMEI == request ||
             RIL_REQUEST_BASEBAND_VERSION == request ||
             RIL_REQUEST_GET_CURRENT_CALLS == request)){
        LOGD("[GSM]:radio state =%d, onRequest: %s\r\n", sState, requestToString(request));
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
        return;
    }

    switch (request) {

        /**** call ****/
        //--------------------------------------------------------
        case RIL_REQUEST_GET_CURRENT_CALLS:
            requestGetCurrentCalls(data, datalen, t);
            break;
            
        case RIL_REQUEST_DIAL:
            requestDial(data, datalen, t);
            break;
            
        case RIL_REQUEST_HANGUP:
            requestHangup(data, datalen, t);
            break;
            
        case RIL_REQUEST_HANGUP_WAITING_OR_BACKGROUND:
            at_send_command("AT+CHLD=0", NULL);

            RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
            break;
            
        case RIL_REQUEST_HANGUP_FOREGROUND_RESUME_BACKGROUND:
            at_send_command("AT+CHLD=1", NULL);

            RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
            break;
            
        case RIL_REQUEST_SWITCH_WAITING_OR_HOLDING_AND_ACTIVE:
            at_send_command("AT+CHLD=2", NULL);

            RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
            break;
            
        case RIL_REQUEST_ANSWER:
            at_send_command("ATA", NULL);

            RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
            break;
            
        case RIL_REQUEST_CONFERENCE:
            at_send_command("AT+CHLD=3", NULL);

            RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
            break;
            
        case RIL_REQUEST_UDUB:
            at_send_command("ATH", NULL);

            RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
            break;

        case RIL_REQUEST_SEPARATE_CONNECTION:
            {
                char  cmd[12];
                int   party = ((int*)data)[0];

                if (party > 0 && party < 10) {
                    sprintf(cmd, "AT+CHLD=2%d", party);
                    at_send_command(cmd, NULL);
                    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
                } else {
                    LOGD("[GSM]:%s error for RIL_REQUEST_SEPARATE_CONNECTION!\n", __func__);
                    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
                }
            }
            break;
        //--------------------------------------------------------    

        /**** sms ****/
        //--------------------------------------------------------
        case RIL_REQUEST_SEND_SMS:
            requestSendSMS(data, datalen, t);
            break;

        case RIL_REQUEST_SMS_ACKNOWLEDGE:
            requestSMSAcknowledge(data, datalen, t);
            break;

        case RIL_REQUEST_WRITE_SMS_TO_SIM:
            requestWriteSmsToSim(data, datalen, t);
            break;

        case RIL_REQUEST_DELETE_SMS_ON_SIM: {
            char * cmd;
            p_response = NULL;
            asprintf(&cmd, "AT+CMGD=%d", ((int *)data)[0]);
            err = at_send_command(cmd, &p_response);
            free(cmd);
            if (err < 0 || p_response->success == 0) {
                LOGE("[GSM]: %s error for RIL_REQUEST_DELETE_SMS_ON_SIM!\n", __func__);
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            } else {
                RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
            }
            at_response_free(p_response);
            break;
        }
        //--------------------------------------------------------

        /**** data connection ****/
        //--------------------------------------------------------
        case RIL_REQUEST_DEACTIVATE_DATA_CALL: {
            requestDeactivateDefaultPDP(data, datalen, t);
            break;
        }

        case RIL_REQUEST_LAST_DATA_CALL_FAIL_CAUSE: {
            requestLastPDPFailCause(t);
            break;
        }

        case RIL_REQUEST_SETUP_DATA_CALL:
            requestSetupDataCall(data, datalen, t);
            break;

        case RIL_REQUEST_DATA_CALL_LIST:
            requestDataCallList(data, datalen, t);
            break;
        //--------------------------------------------------------    

        /**** network ****/
        //--------------------------------------------------------
        case RIL_REQUEST_SET_PREFERRED_NETWORK_TYPE: {
            requestSetPreferredNetworkType(data,datalen,t);
            break;
        }

        case RIL_REQUEST_SIGNAL_STRENGTH:
            requestSignalStrength(data, datalen, t);
            break;
            
        case RIL_REQUEST_VOICE_REGISTRATION_STATE:
        case RIL_REQUEST_DATA_REGISTRATION_STATE:
            requestRegistrationState(request, data, datalen, t);
            break;
            
        case RIL_REQUEST_OPERATOR:
            requestOperator(data, datalen, t);
            break;

        case RIL_REQUEST_SET_NETWORK_SELECTION_AUTOMATIC:
            at_send_command("AT+COPS=0", NULL);
            break;

        case RIL_REQUEST_QUERY_NETWORK_SELECTION_MODE:
            requestQueryNetworkSelectionMode(data, datalen, t);
            break;
        //--------------------------------------------------------    

        /**** sim card ****/
        //--------------------------------------------------------
        case RIL_REQUEST_GET_SIM_STATUS: {
            RIL_CardStatus_v6 *p_card_status;
            char *p_buffer;
            int buffer_size;

            int result = getCardStatus(&p_card_status);
            if (result == RIL_E_SUCCESS) {
                p_buffer = (char *)p_card_status;
                buffer_size = sizeof(*p_card_status);
            } else {
                p_buffer = NULL;
                buffer_size = 0;
            }
            RIL_onRequestComplete(t, result, p_buffer, buffer_size);
            freeCardStatus(p_card_status);
            break;
        }

         case RIL_REQUEST_GET_IMSI:
            p_response = NULL;
            err = at_send_command_numeric("AT+CIMI", &p_response);

            if (err < 0 || p_response->success == 0) {
                LOGE("[GSM]: %s error for RIL_REQUEST_GET_IMSI!\n", __func__);
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            } else {
                RIL_onRequestComplete(t, RIL_E_SUCCESS,
                    p_response->p_intermediates->line, sizeof(char *));
            }
            at_response_free(p_response);
            break;

         case RIL_REQUEST_SIM_IO:
            requestSIM_IO(data,datalen,t);
            break;

        case RIL_REQUEST_ENTER_SIM_PIN:
        case RIL_REQUEST_ENTER_SIM_PUK:
        case RIL_REQUEST_ENTER_SIM_PIN2:
        case RIL_REQUEST_ENTER_SIM_PUK2:
            requestEnterSimPin(data, datalen, t);
            break;

        /**
         *  Wythe add on 2014-1-26 ->start
         *  Handle request about SIM lock.
         */
        case RIL_REQUEST_CHANGE_SIM_PIN:
            requestChangePassword(data, datalen, t, "SC", request);
            break;
        case RIL_REQUEST_CHANGE_SIM_PIN2:
            requestChangePassword(data, datalen, t, "P2", request);
            break;
		case RIL_REQUEST_QUERY_FACILITY_LOCK:
			requestQueryFacilityLock(data, datalen, t);
			break;
		case RIL_REQUEST_SET_FACILITY_LOCK:
			requestSetFacilityLock(data, datalen, t);
        /**
         *  Wythe add on 2014-1-26 ->end
         *  Handle request about SIM lock.
         */            
			break;
        //--------------------------------------------------------    
            
        /**** other,e.g. system, baseband information****/
        case RIL_REQUEST_SCREEN_STATE: {
            requestScreenState(data,datalen,t);
            break;
        }

        case RIL_REQUEST_BASEBAND_VERSION: {
            requestBaseBandVersion(data,datalen,t);
            break;
        }

        case RIL_REQUEST_GET_IMEISV: {
            requestGetIMEISV(t);
            break;
        }

        case RIL_REQUEST_REPORT_STK_SERVICE_IS_RUNNING: {
            requestReportSTKServiceIsRunning(t);
            break;
        }

        case RIL_REQUEST_RADIO_POWER:
            requestRadioPower(data, datalen, t);
            break;
        case RIL_REQUEST_DTMF: {
            char c = ((char *)data)[0];
            char *cmd;
            asprintf(&cmd, "AT+VTS=%c", (int)c);
            at_send_command(cmd, NULL);
            free(cmd);
            RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
            break;
        }

        case RIL_REQUEST_GET_IMEI:
            p_response = NULL;
            err = at_send_command_numeric("AT+CGSN", &p_response);

            if (err < 0 || p_response->success == 0) {
                LOGE("[GSM]: %s error for RIL_REQUEST_GET_IMEI!\n", __func__);
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            } else {
                RIL_onRequestComplete(t, RIL_E_SUCCESS,
                    p_response->p_intermediates->line, sizeof(char *));
            }
            at_response_free(p_response);
            break;

        case RIL_REQUEST_SEND_USSD:
            requestSendUSSD(data, datalen, t);
            break;

        case RIL_REQUEST_CANCEL_USSD:
            p_response = NULL;
            err = at_send_command_numeric("AT+CUSD=2", &p_response);

            if (err < 0 || p_response->success == 0) {
                LOGE("[GSM]: %s error for RIL_REQUEST_CANCEL_USSD!\n", __func__);
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            } else {
                RIL_onRequestComplete(t, RIL_E_SUCCESS,
                    p_response->p_intermediates->line, sizeof(char *));
            }
            at_response_free(p_response);
            break;

        case RIL_REQUEST_OEM_HOOK_RAW:
            // echo back data
            RIL_onRequestComplete(t, RIL_E_SUCCESS, data, datalen);
            break;


        case RIL_REQUEST_OEM_HOOK_STRINGS: {
            int i;
            const char ** cur;

            LOGD("got OEM_HOOK_STRINGS: 0x%8p %lu", data, (long)datalen);


            for (i = (datalen / sizeof (char *)), cur = (const char **)data ;
                    i > 0 ; cur++, i --) {
                LOGD("> '%s'", *cur);
            }

            // echo back strings
            RIL_onRequestComplete(t, RIL_E_SUCCESS, data, datalen);
            break;
        }

        default:
            RIL_onRequestComplete(t, RIL_E_REQUEST_NOT_SUPPORTED, NULL, 0);
            break;
    }
}

/**
 * Synchronous call from the RIL to us to return current radio state.
 * RADIO_STATE_UNAVAILABLE should be the initial state.
 */
static RIL_RadioState
currentState()
{
    return sState;
}
/**
 * Call from RIL to us to find out whether a specific request code
 * is supported by this implementation.
 *
 * Return 1 for "supported" and 0 for "unsupported"
 */

static int
onSupports (int requestCode)
{
    //@@@ todo

    return 1;
}

static void onCancel (RIL_Token t)
{
    //@@@todo

}

static const char * getVersion(void)
{
    return "android reference-ril 1.0";
}

static void
setRadioState(RIL_RadioState newState)
{
    RIL_RadioState oldState;

    pthread_mutex_lock(&s_state_mutex);

    oldState = sState;

    if (s_closed > 0) {
        // If we're closed, the only reasonable state is
        // RADIO_STATE_UNAVAILABLE
        // This is here because things on the main thread
        // may attempt to change the radio state after the closed
        // event happened in another thread
        newState = RADIO_STATE_UNAVAILABLE;
    }

    if (sState != newState || s_closed > 0) {
        sState = newState;

        pthread_cond_broadcast (&s_state_cond);
    }

    pthread_mutex_unlock(&s_state_mutex);


    /* do these outside of the mutex */
    if (sState != oldState || 
        // add by wythe on 2014-1-26 
        sState == RADIO_STATE_SIM_LOCKED_OR_ABSENT) {
        RIL_onUnsolicitedResponse (RIL_UNSOL_RESPONSE_RADIO_STATE_CHANGED,
                                    NULL, 0);

        /* FIXME onSimReady() and onRadioPowerOn() cannot be called
         * from the AT reader thread
         * Currently, this doesn't happen, but if that changes then these
         * will need to be dispatched on the request thread
         */
        if (sState == RADIO_STATE_SIM_READY) {
            onSIMReady();
        } else if (sState == RADIO_STATE_SIM_NOT_READY) {
            onRadioPowerOn();
        }
    }
}

/**
 *  Modify by wythe on 2014-1-26 ->start
 */

typedef enum {
    SIM_ABSENT = 0,                     /* SIM card is not inserted */
    SIM_NOT_READY = 1,                  /* SIM card is not ready */
    SIM_READY = 2,                      /* radiostate = RADIO_STATE_SIM_READY */
    SIM_PIN = 3,                        /* SIM PIN code lock */
    SIM_PUK = 4,                        /* SIM PUK code lock */
    SIM_NETWORK_PERSO = 5,              /* Network Personalization lock */
    SIM_PIN2 = 6,                       /* SIM PIN2 lock */
    SIM_PUK2 = 7,                       /* SIM PUK2 lock */
    SIM_NETWORK_SUBSET_PERSO = 8,       /* Network Subset Personalization */
    SIM_SERVICE_PROVIDER_PERSO = 9,     /* Service Provider Personalization */
    SIM_CORPORATE_PERSO = 10,           /* Corporate Personalization */
    SIM_SIM_PERSO = 11,                 /* SIM/USIM Personalization */
    SIM_STERICSSON_LOCK = 12,           /* ST-Ericsson Extended SIM */
    SIM_BLOCKED = 13,                   /* SIM card is blocked */
    SIM_PERM_BLOCKED = 14,              /* SIM card is permanently blocked */
    SIM_NETWORK_PERSO_PUK = 15,         /* Network Personalization PUK */
    SIM_NETWORK_SUBSET_PERSO_PUK = 16,  /* Network Subset Perso. PUK */
    SIM_SERVICE_PROVIDER_PERSO_PUK = 17,/* Service Provider Perso. PUK */
    SIM_CORPORATE_PERSO_PUK = 18,       /* Corporate Personalization PUK */
    SIM_SIM_PERSO_PUK = 19,             /* SIM Personalization PUK (unused) */
    SIM_PUK2_PERM_BLOCKED = 20          /* PUK2 is permanently blocked */
} SIM_Status;

static SIM_Status getSIMStatus()
{
    ATResponse *atResponse = NULL;
    int err;
    SIM_Status ret = SIM_ABSENT;
    char *cpinLine;
    char *cpinResult;

    if (sState == RADIO_STATE_OFF ||
        sState == RADIO_STATE_UNAVAILABLE) {
        return SIM_NOT_READY;
    }

    err = at_send_command_singleline("AT+CPIN?", "+CPIN:", &atResponse);

    if (err != 0) {
        if (at_get_error_type(err) == AT_ERROR) {
            ret = SIM_NOT_READY;
						goto done;
				}
        switch (at_get_cme_error(atResponse)) {
        case CME_SIM_NOT_INSERTED:
            ret = SIM_ABSENT;
            break;
        case CME_SIM_PIN_REQUIRED:
            ret = SIM_PIN;
            break;
        case CME_SIM_PUK_REQUIRED:
            ret = SIM_PUK;
            break;
        case CME_SIM_PIN2_REQUIRED:
            ret = SIM_PIN2;
            break;
        case CME_SIM_PUK2_REQUIRED:
            ret = SIM_PUK2;
            break;
        case CME_NETWORK_PERSONALIZATION_PIN_REQUIRED:
            ret = SIM_NETWORK_PERSO;
            break;
        case CME_NETWORK_PERSONALIZATION_PUK_REQUIRED:
            ret = SIM_NETWORK_PERSO_PUK;
            break;
        case CME_NETWORK_SUBSET_PERSONALIZATION_PIN_REQUIRED:
            ret = SIM_NETWORK_SUBSET_PERSO;
            break;
        case CME_NETWORK_SUBSET_PERSONALIZATION_PUK_REQUIRED:
            ret = SIM_NETWORK_SUBSET_PERSO_PUK;
            break;
        case CME_SERVICE_PROVIDER_PERSONALIZATION_PIN_REQUIRED:
            ret = SIM_SERVICE_PROVIDER_PERSO;
            break;
        case CME_SERVICE_PROVIDER_PERSONALIZATION_PUK_REQUIRED:
            ret = SIM_SERVICE_PROVIDER_PERSO_PUK;
            break;
#if 0            
        case CME_PH_SIMLOCK_PIN_REQUIRED: /* PUK not in use by modem */
            ret = SIM_SIM_PERSO;
            goto done;
#endif            
        case CME_CORPORATE_PERSONALIZATION_PIN_REQUIRED:
            ret = SIM_CORPORATE_PERSO;
            break;
        case CME_CORPORATE_PERSONALIZATION_PUK_REQUIRED:
            ret = SIM_CORPORATE_PERSO_PUK;
            break;
        default:
            ret = SIM_NOT_READY;
            break;
        }
        return ret;
    }

    /* CPIN? has succeeded, now look at the result. */
		if(NULL == atResponse->p_intermediates){
				ret = SIM_NOT_READY;
				goto done;		
		}
    cpinLine = atResponse->p_intermediates->line;
    err = at_tok_start(&cpinLine);

    LOGI("cpinLine=%s\r\n", cpinLine);
    if (err < 0) {
        LOGI("SIM_NOT_READY in at_tok_start(&cpinLine).");
        ret = SIM_NOT_READY;
        goto done;
    }

    err = at_tok_nextstr(&cpinLine, &cpinResult);
    LOGI("cpinResult=%s\r\n", cpinResult);
    if (err < 0) {
        LOGI("SIM_NOT_READY in at_tok_nextstr(&cpinLine, &cpinResult).");
        ret = SIM_NOT_READY;
        goto done;
    }

    if (0 == strcmp(cpinResult, "READY")) {
        ret = SIM_READY;
    } else if (0 == strcmp(cpinResult, "SIM PIN")) {
        ret = SIM_PIN;
    } else if (0 == strcmp(cpinResult, "SIM PUK")) {
        ret = SIM_PUK;
    } else if (0 == strcmp(cpinResult, "SIM PIN2")) {
        ret = SIM_PIN2;
    } else if (0 == strcmp(cpinResult, "SIM PUK2")) {
        ret = SIM_PUK2;
    } else if (0 == strcmp(cpinResult, "PH-NET PIN")) {
        ret = SIM_NETWORK_PERSO;
    } else if (0 == strcmp(cpinResult, "PH-NETSUB PIN")) {
        ret = SIM_NETWORK_SUBSET_PERSO;
    } else if (0 == strcmp(cpinResult, "PH-SP PIN")) {
        ret = SIM_SERVICE_PROVIDER_PERSO;
    } else if (0 == strcmp(cpinResult, "PH-CORP PIN")) {
        ret = SIM_CORPORATE_PERSO;
    } else if (0 == strcmp(cpinResult, "PH-SIMLOCK PIN")) {
        ret = SIM_SIM_PERSO;
    } else if (0 == strcmp(cpinResult, "PH-ESL PIN")) {
        ret = SIM_STERICSSON_LOCK;
    } else if (0 == strcmp(cpinResult, "BLOCKED")) {
        int numRetries = 3;
        if (numRetries == -1 || numRetries == 0)
            ret = SIM_PERM_BLOCKED;
        else
            ret = SIM_PUK2_PERM_BLOCKED;
    } else if (0 == strcmp(cpinResult, "PH-SIM PIN")) {
        /*
         * Should not happen since lock must first be set from the phone.
         * Setting this lock is not supported by Android.
         */
        ret = SIM_BLOCKED;
    } else {
        /* Unknown locks should not exist. Defaulting to "sim absent" */
        ret = SIM_ABSENT;
    }
done:
    at_response_free(atResponse);
    return ret;
}


/**
 *  Add on 2014-1-26
 *  Define more card status. This card status will be supported
 *  by modem side.
 */

static const RIL_AppStatus app_status_array[] = {
    
    // SIM_ABSENT = 0
    { RIL_APPTYPE_UNKNOWN, RIL_APPSTATE_UNKNOWN, RIL_PERSOSUBSTATE_UNKNOWN,
      NULL, NULL, 0, RIL_PINSTATE_UNKNOWN, RIL_PINSTATE_UNKNOWN },
      
    // SIM_NOT_READY = 1
    { RIL_APPTYPE_SIM, RIL_APPSTATE_DETECTED, RIL_PERSOSUBSTATE_UNKNOWN,
      NULL, NULL, 0, RIL_PINSTATE_UNKNOWN, RIL_PINSTATE_UNKNOWN },
      
    // SIM_READY = 2
    { RIL_APPTYPE_SIM, RIL_APPSTATE_READY, RIL_PERSOSUBSTATE_READY,
      NULL, NULL, 0, RIL_PINSTATE_UNKNOWN, RIL_PINSTATE_UNKNOWN },
      
    // SIM_PIN = 3
    { RIL_APPTYPE_SIM, RIL_APPSTATE_PIN, RIL_PERSOSUBSTATE_UNKNOWN,
      NULL, NULL, 0, RIL_PINSTATE_ENABLED_NOT_VERIFIED, RIL_PINSTATE_UNKNOWN },
      
    // SIM_PUK = 4
    { RIL_APPTYPE_SIM, RIL_APPSTATE_PUK, RIL_PERSOSUBSTATE_UNKNOWN,
      NULL, NULL, 0, RIL_PINSTATE_ENABLED_BLOCKED, RIL_PINSTATE_UNKNOWN },
      
    // SIM_NETWORK_PERSONALIZATION = 5
    { RIL_APPTYPE_SIM, RIL_APPSTATE_SUBSCRIPTION_PERSO, RIL_PERSOSUBSTATE_SIM_NETWORK,
      NULL, NULL, 0, RIL_PINSTATE_ENABLED_NOT_VERIFIED, RIL_PINSTATE_UNKNOWN },

    //SIM_PIN2 = 6
    { RIL_APPTYPE_SIM, RIL_APPSTATE_READY, RIL_PERSOSUBSTATE_UNKNOWN,
      NULL, NULL, 0, RIL_PINSTATE_UNKNOWN, RIL_PINSTATE_ENABLED_NOT_VERIFIED},

    //SIM_PUK2 = 7
    { RIL_APPTYPE_SIM, RIL_APPSTATE_READY, RIL_PERSOSUBSTATE_UNKNOWN,
      NULL, NULL, 0, RIL_PINSTATE_UNKNOWN, RIL_PINSTATE_ENABLED_BLOCKED},

    //SIM_NETWORK_SUBSET_PERSO = 8
    { RIL_APPTYPE_SIM, RIL_APPSTATE_SUBSCRIPTION_PERSO, RIL_PERSOSUBSTATE_SIM_NETWORK_SUBSET,
      NULL, NULL, 0, RIL_PINSTATE_ENABLED_NOT_VERIFIED, RIL_PINSTATE_UNKNOWN},

    //SIM_SERVICE_PROVIDER_PERSO = 9
    { RIL_APPTYPE_SIM, RIL_APPSTATE_SUBSCRIPTION_PERSO, RIL_PERSOSUBSTATE_SIM_SERVICE_PROVIDER,
      NULL, NULL, 0, RIL_PINSTATE_ENABLED_NOT_VERIFIED, RIL_PINSTATE_UNKNOWN},

    //SIM_CORPORATE_PERSO = 10
    { RIL_APPTYPE_SIM, RIL_APPSTATE_SUBSCRIPTION_PERSO, RIL_PERSOSUBSTATE_SIM_CORPORATE,
      NULL, NULL, 0, RIL_PINSTATE_ENABLED_NOT_VERIFIED, RIL_PINSTATE_UNKNOWN},

    //SIM_SIM_PERSO = 11
    { RIL_APPTYPE_SIM, RIL_APPSTATE_SUBSCRIPTION_PERSO, RIL_PERSOSUBSTATE_SIM_SIM,
      NULL, NULL, 0, RIL_PINSTATE_ENABLED_NOT_VERIFIED, RIL_PINSTATE_UNKNOWN},

    //SIM_STERICSSON_LOCK = 12
    { RIL_APPTYPE_SIM, RIL_APPSTATE_SUBSCRIPTION_PERSO, RIL_PERSOSUBSTATE_UNKNOWN,
      NULL, NULL, 0, RIL_PINSTATE_ENABLED_NOT_VERIFIED, RIL_PINSTATE_UNKNOWN},   
      
    //SIM_BLOCKED = 13
    { RIL_APPTYPE_SIM, RIL_APPSTATE_UNKNOWN, RIL_PERSOSUBSTATE_UNKNOWN,
      NULL, NULL, 0, RIL_PINSTATE_ENABLED_BLOCKED, RIL_PINSTATE_UNKNOWN},       

    //SIM_PERM_BLOCKED = 14
    { RIL_APPTYPE_SIM, RIL_APPSTATE_UNKNOWN, RIL_PERSOSUBSTATE_UNKNOWN,
      NULL, NULL, 0, RIL_PINSTATE_ENABLED_PERM_BLOCKED, RIL_PINSTATE_UNKNOWN},  

    //SIM_NETWORK_PERSO_PUK = 15
    { RIL_APPTYPE_SIM, RIL_APPSTATE_SUBSCRIPTION_PERSO, RIL_PERSOSUBSTATE_SIM_NETWORK_PUK,
      NULL, NULL, 0, RIL_PINSTATE_ENABLED_NOT_VERIFIED, RIL_PINSTATE_UNKNOWN},  

    //SIM_NETWORK_SUBSET_PERSO_PUK = 16
    { RIL_APPTYPE_SIM, RIL_APPSTATE_SUBSCRIPTION_PERSO, RIL_PERSOSUBSTATE_SIM_NETWORK_SUBSET_PUK,
      NULL, NULL, 0, RIL_PINSTATE_ENABLED_NOT_VERIFIED, RIL_PINSTATE_UNKNOWN},   

    //SIM_SERVICE_PROVIDER_PERSO_PUK = 17
    { RIL_APPTYPE_SIM, RIL_APPSTATE_SUBSCRIPTION_PERSO, RIL_PERSOSUBSTATE_SIM_SERVICE_PROVIDER_PUK,
      NULL, NULL, 0, RIL_PINSTATE_ENABLED_NOT_VERIFIED, RIL_PINSTATE_UNKNOWN},  

    //SIM_CORPORATE_PERSO_PUK = 18
    { RIL_APPTYPE_SIM, RIL_APPSTATE_SUBSCRIPTION_PERSO, RIL_PERSOSUBSTATE_SIM_CORPORATE_PUK,
      NULL, NULL, 0, RIL_PINSTATE_ENABLED_NOT_VERIFIED, RIL_PINSTATE_UNKNOWN},   

    //SIM_SIM_PERSO_PUK = 19
    { RIL_APPTYPE_SIM, RIL_APPSTATE_SUBSCRIPTION_PERSO, RIL_PERSOSUBSTATE_SIM_SIM_PUK,
      NULL, NULL, 0, RIL_PINSTATE_ENABLED_NOT_VERIFIED, RIL_PINSTATE_UNKNOWN},   

    //SIM_PUK2_PERM_BLOCKED = 20
    { RIL_APPTYPE_SIM, RIL_APPSTATE_UNKNOWN, RIL_PERSOSUBSTATE_UNKNOWN,
      NULL, NULL, 0, RIL_PINSTATE_UNKNOWN, RIL_PINSTATE_ENABLED_PERM_BLOCKED}    
};

/**
 *  Modify by wythe on 2014-1-26 ->end
 */

/**
 * Get the current card status.
 *
 * This must be freed using freeCardStatus.
 * @return: On success returns RIL_E_SUCCESS
 */
static int getCardStatus(RIL_CardStatus_v6 **pp_card_status) {

    RIL_CardState card_state;
    int num_apps;

    int sim_status = getSIMStatus();
    if (sim_status == SIM_ABSENT) {
        card_state = RIL_CARDSTATE_ABSENT;
        num_apps = 0;
    } else {
        card_state = RIL_CARDSTATE_PRESENT;
        num_apps = 1;
    }

    // Allocate and initialize base card status.
    RIL_CardStatus_v6 *p_card_status = malloc(sizeof(RIL_CardStatus_v6));
    p_card_status->card_state = card_state;
    p_card_status->universal_pin_state = RIL_PINSTATE_UNKNOWN;
    p_card_status->gsm_umts_subscription_app_index = RIL_CARD_MAX_APPS;
    p_card_status->cdma_subscription_app_index = RIL_CARD_MAX_APPS;
    p_card_status->ims_subscription_app_index = RIL_CARD_MAX_APPS;
    p_card_status->num_applications = num_apps;

    // Initialize application status
    int i;
    for (i = 0; i < RIL_CARD_MAX_APPS; i++) {
        p_card_status->applications[i] = app_status_array[SIM_ABSENT];
    }

    // Pickup the appropriate application status
    // that reflects sim_status for gsm.
    if (num_apps != 0) {
        // Only support one app, gsm
        p_card_status->num_applications = 1;
        p_card_status->gsm_umts_subscription_app_index = 0;

        // Get the correct app status
        p_card_status->applications[0] = app_status_array[sim_status];
    }

    *pp_card_status = p_card_status;
    return RIL_E_SUCCESS;
}

/**
 * Free the card status returned by getCardStatus
 */
static void freeCardStatus(RIL_CardStatus_v6 *p_card_status) {
    free(p_card_status);
}

/**
 * SIM ready means any commands that access the SIM will work, including:
 *  AT+CPIN, AT+CSMS, AT+CNMI, AT+CRSM
 *  (all SMS-related commands)
 */
static void pollSIMState(void *param)
{
    if (((int) param) != 1 &&
        sState!= RADIO_STATE_SIM_NOT_READY &&
        sState != RADIO_STATE_SIM_LOCKED_OR_ABSENT)
        /* No longer valid to poll. */
        return;

    switch (getSIMStatus()) {
    case SIM_NOT_READY:
        LOGI("SIM_NOT_READY, poll for sim state.");
        RIL_requestTimedCallback (pollSIMState, NULL, &TIMEVAL_SIMPOLL);
        return;

    case SIM_PIN2:
    case SIM_PUK2:
    case SIM_PUK2_PERM_BLOCKED:
    case SIM_READY:
        setRadioState(RADIO_STATE_SIM_READY);
        return;
    case SIM_ABSENT:
    case SIM_PIN:
    case SIM_PUK:
    case SIM_NETWORK_PERSO:
    case SIM_NETWORK_SUBSET_PERSO:
    case SIM_SERVICE_PROVIDER_PERSO:
    case SIM_CORPORATE_PERSO:
    case SIM_SIM_PERSO:
    case SIM_STERICSSON_LOCK:
    case SIM_BLOCKED:
    case SIM_PERM_BLOCKED:
    case SIM_NETWORK_PERSO_PUK:
    case SIM_NETWORK_SUBSET_PERSO_PUK:
    case SIM_SERVICE_PROVIDER_PERSO_PUK:
    case SIM_CORPORATE_PERSO_PUK:
    /* pass through, do not break */
    default:
        LOGI("Set RADIO_STATE_SIM_LOCKED_OR_ABSENT.");
        setRadioState(RADIO_STATE_SIM_LOCKED_OR_ABSENT);
        return;
    }
} 

/** returns 1 if on, 0 if off, and -1 on error */
static int isRadioOn()
{
    ATResponse *p_response = NULL;
    int err;
    char *line;
    char ret;

    err = at_send_command_singleline("AT+CFUN?", "+CFUN:", &p_response);

    if (err < 0 || p_response->success == 0) {
        // assume radio is off
        goto error;
    }

    line = p_response->p_intermediates->line;

    err = at_tok_start(&line);
    if (err < 0) goto error;

    err = at_tok_nextbool(&line, &ret);
    if (err < 0) goto error;

    at_response_free(p_response);

    return (int)ret;

error:
    LOGE("[GSM]:isRadioOn return error!\n");
    at_response_free(p_response);
    return -1;
}

/**
 * Initialize everything that can be configured while we're still in
 * AT+CFUN=0
 */
static void initializeCallback(void *param)
{
    ATResponse *p_response = NULL;
    int err;

    setRadioState (RADIO_STATE_OFF);

    if(at_handshake() < 0){
        LOGE("[GSM]:at_handshake failed!\n");
    }

    /* note: we don't check errors here. Everything important will
       be handled in onATTimeout and onATReaderClosed */

    /*  atchannel is tolerant of echo but it must */
    /*  have verbose result codes */
    at_send_command("ATE0Q0V1", NULL);

    /*  No auto-answer */
    at_send_command("ATS0=0", NULL);

    /*  Extended errors */
    at_send_command("AT+CMEE=1", NULL);

    /*  Network registration events */
    err = at_send_command("AT+CREG=2", &p_response);

    /* some handsets -- in tethered mode -- don't support CREG=2 */
    if (err < 0 || p_response->success == 0) {
        at_send_command("AT+CREG=1", NULL);
    }

    at_response_free(p_response);

    /*  GPRS registration events */
    at_send_command("AT+CGREG=1", NULL);

    /*  Call Waiting notifications */
    at_send_command("AT+CCWA=1", NULL);

    /*  Alternating voice/data off */
    at_send_command("AT+CMOD=0", NULL);

    /*  Set muted */
    //at_send_command("AT+CMUT=1", NULL);

    /*  +CSSU unsolicited supp service notifications */
    at_send_command("AT+CSSN=0,1", NULL);

    /*  no connected line identification */
    at_send_command("AT+COLP=0", NULL);

    /*  HEX character set */
    at_send_command("AT+CSCS=\"HEX\"", NULL);

    /*  USSD unsolicited */
    at_send_command("AT+CUSD=1", NULL);

    /*  Enable +CGEV GPRS event notifications, but don't buffer */
    
    /**
     *  Wythe: Modify on 2013-04-02 for 4.0 ril
     *  we support AT+CGEREP=1
     */
	at_send_command("AT+CGEREP=1", NULL);

    /*  SMS PDU mode */
    at_send_command("AT+CMGF=0", NULL);

    /* assume radio is off on error */
    if (isRadioOn() > 0) {
        setRadioState (RADIO_STATE_SIM_NOT_READY);
    }
    else
    {
        at_send_command("AT+CFUN=1", NULL);
    }
}

static void waitForClose()
{
    pthread_mutex_lock(&s_state_mutex);

    while (s_closed == 0) {
        pthread_cond_wait(&s_state_cond, &s_state_mutex);
    }

    pthread_mutex_unlock(&s_state_mutex);
}

/**
 * Called by atchannel when an unsolicited line appears
 * This is called on atchannel's reader thread. AT commands may
 * not be issued here
 */
static void onUnsolicited (const char *s, const char *sms_pdu)
{
    char *line = NULL;
    int err;

    /* Ignore unsolicited responses until we're initialized.
     * This is OK because the RIL library will poll for initial state
     */
    if (sState == RADIO_STATE_UNAVAILABLE) {
        return;
    }

    if (strStartsWith(s, "%CTZV:")) {
        /* TI specific -- NITZ time */
        char *response;

        line = strdup(s);
        at_tok_start(&line);

        err = at_tok_nextstr(&line, &response);

        if (err != 0) {
            LOGE("invalid NITZ line %s\n", s);
        } else {
            RIL_onUnsolicitedResponse (
                RIL_UNSOL_NITZ_TIME_RECEIVED,
                response, strlen(response));
        }
    } else if (strStartsWith(s,"+CRING:")
                || strStartsWith(s,"RING")
                || strStartsWith(s,"NO CARRIER")
                || strStartsWith(s,"+CCWA")
    ) {
        RIL_onUnsolicitedResponse (
            RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED,
            NULL, 0);
#ifdef WORKAROUND_FAKE_CGEV
        RIL_requestTimedCallback (onDataCallListChanged, NULL, NULL); //TODO use new function
#endif /* WORKAROUND_FAKE_CGEV */
    } else if (strStartsWith(s,"+CREG:")
                || strStartsWith(s,"+CGREG:")
    ) {
        RIL_onUnsolicitedResponse (
            RIL_UNSOL_RESPONSE_VOICE_NETWORK_STATE_CHANGED,
            NULL, 0);
#ifdef WORKAROUND_FAKE_CGEV
        RIL_requestTimedCallback (onDataCallListChanged, NULL, NULL);
#endif /* WORKAROUND_FAKE_CGEV */
    } else if (strStartsWith(s, "+CMT:")) {
        RIL_onUnsolicitedResponse (
            RIL_UNSOL_RESPONSE_NEW_SMS,
            sms_pdu, strlen(sms_pdu));
    } else if (strStartsWith(s, "+CDS:")) {
        RIL_onUnsolicitedResponse (
            RIL_UNSOL_RESPONSE_NEW_SMS_STATUS_REPORT,
            sms_pdu, strlen(sms_pdu));
    } else if (strStartsWith(s, "+CGEV:")) {
        /* Really, we can ignore NW CLASS and ME CLASS events here,
         * but right now we don't since extranous
         * RIL_UNSOL_DATA_CALL_LIST_CHANGED calls are tolerated
         */
        /* can't issue AT commands here -- call on main thread */
        RIL_requestTimedCallback (onDataCallListChanged, NULL, NULL);
#ifdef WORKAROUND_FAKE_CGEV
    } else if (strStartsWith(s, "+CME ERROR: 150")) {
        RIL_requestTimedCallback (onDataCallListChanged, NULL, NULL);
#endif /* WORKAROUND_FAKE_CGEV */
    } else if (strStartsWith(s, "+QUSIM:") || strStartsWith(s, "+QIND: SMS DONE") || strStartsWith(s, "+QIND: PB DONE"))
	{
		setRadioState(RADIO_STATE_SIM_READY);
		RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_RADIO_STATE_CHANGED, NULL, 0);
	}
}

/* Called on command or reader thread */
static void onATReaderClosed()
{
    LOGI("AT channel closed\n");
    at_close();
    s_closed = 1;

    setRadioState (RADIO_STATE_UNAVAILABLE);
}

/* Called on command thread */
static void onATTimeout()
{
    LOGI("AT channel timeout; closing\n");
    at_close();

    s_closed = 1;

    /* FIXME cause a radio reset here */

    setRadioState (RADIO_STATE_UNAVAILABLE);
}

static void usage(char *s)
{
#ifdef RIL_SHLIB
    fprintf(stderr, "reference-ril requires: -p <tcp port> or -d /dev/tty_device\n");
#else
    fprintf(stderr, "usage: %s [-p <tcp port>] [-d /dev/tty_device]\n", s);
    exit(-1);
#endif
}

static void *
mainLoop(void *param)
{
    int fd;
    int ret;

    static int time2chmod = 1;
     
    AT_DUMP("== ", "entering mainLoop()", -1 );
    at_set_on_reader_closed(onATReaderClosed);
    at_set_on_timeout(onATTimeout);

    #define ANDROID_CMUX_DEV_DESC_0     "/dev/chn/1"
    #define ANDROID_CMUX_DEV_DESC_1     "/dev/chn/2"

    if(time2chmod){
        chmod(ANDROID_CMUX_DEV_DESC_0, 0777);
        chmod(ANDROID_CMUX_DEV_DESC_1, 0777);
        time2chmod = 0;
    }

    for (;;) {
        fd = -1;
        while  (fd < 0) {
            if (s_port > 0) {
                fd = socket_loopback_client(s_port, SOCK_STREAM);
            } else if (s_device_socket) {
                if (!strcmp(s_device_path, "/dev/socket/qemud")) {
                    /* Before trying to connect to /dev/socket/qemud (which is
                     * now another "legacy" way of communicating with the
                     * emulator), we will try to connecto to gsm service via
                     * qemu pipe. */
                    fd = qemu_pipe_open("qemud:gsm");
                    if (fd < 0) {
                        /* Qemu-specific control socket */
                        fd = socket_local_client( "qemud",
                                                  ANDROID_SOCKET_NAMESPACE_RESERVED,
                                                  SOCK_STREAM );
                        if (fd >= 0 ) {
                            char  answer[2];

                            if ( write(fd, "gsm", 3) != 3 ||
                                 read(fd, answer, 2) != 2 ||
                                 memcmp(answer, "OK", 2) != 0)
                            {
                                close(fd);
                                fd = -1;
                            }
                       }
                    }
                }
                else
                    fd = socket_local_client( s_device_path,
                                            ANDROID_SOCKET_NAMESPACE_FILESYSTEM,
                                            SOCK_STREAM );
            } else if (s_device_path != NULL) {
                fd = open (s_device_path, O_RDWR);
                if ( fd >= 0 && !memcmp( s_device_path, "/dev/chn", 11 ) ) {
                    /* disable echo on serial ports */
                    struct termios  ios;
                    tcgetattr( fd, &ios );
                    ios.c_lflag = 0;  /* disable ECHO, ICANON, etc... */
                    tcsetattr( fd, TCSANOW, &ios );
                }
            }

            if (fd < 0) {
                perror ("opening AT interface. retrying...");
                sleep(10);
                /* never returns */
            }
        }

        s_closed = 0;
        ret = at_open(fd, onUnsolicited);

        if (ret < 0) {
            LOGE ("AT error %d on at_open\n", ret);
            return 0;
        }

        RIL_requestTimedCallback(initializeCallback, NULL, &TIMEVAL_0);

        // Give initializeCallback a chance to dispatched, since
        // we don't presently have a cancellation mechanism
        sleep(1);

        waitForClose();
        LOGI("Re-opening after close");
    }
}

#ifdef RIL_SHLIB

pthread_t s_tid_mainloop;

const RIL_RadioFunctions *RIL_Init(const struct RIL_Env *env, int argc, char **argv)
{
    int ret;
    int fd = -1;
    int opt;
    pthread_attr_t attr;

    s_rilenv = env;

    while ( -1 != (opt = getopt(argc, argv, "p:d:s:"))) {
        switch (opt) {
            case 'p':
                s_port = atoi(optarg);
                if (s_port == 0) {
                    usage(argv[0]);
                    return NULL;
                }
                LOGI("Opening loopback port %d\n", s_port);
            break;

            case 'd':
                s_device_path = optarg;
                LOGI("Opening tty device %s\n", s_device_path);
            break;

            case 's':
                s_device_path   = optarg;
                s_device_socket = 1;
                LOGI("Opening socket %s\n", s_device_path);
            break;

            default:
                usage(argv[0]);
                return NULL;
        }
    }

    if (s_port < 0 && s_device_path == NULL) {
        usage(argv[0]);
        return NULL;
    }

    pthread_attr_init (&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    ret = pthread_create(&s_tid_mainloop, &attr, mainLoop, NULL);

    return &s_callbacks;
}
#else /* RIL_SHLIB */
int main (int argc, char **argv)
{
    int ret;
    int fd = -1;
    int opt;

    while ( -1 != (opt = getopt(argc, argv, "p:d:"))) {
        switch (opt) {
            case 'p':
                s_port = atoi(optarg);
                if (s_port == 0) {
                    usage(argv[0]);
                }
                LOGI("Opening loopback port %d\n", s_port);
            break;

            case 'd':
                s_device_path = optarg;
                LOGI("Opening tty device %s\n", s_device_path);
            break;

            case 's':
                s_device_path   = optarg;
                s_device_socket = 1;
                LOGI("Opening socket %s\n", s_device_path);
            break;

            default:
                usage(argv[0]);
        }
    }

    if (s_port < 0 && s_device_path == NULL) {
        usage(argv[0]);
    }

    RIL_register(&s_callbacks);

    mainLoop(NULL);

    return 0;
}

#endif /* RIL_SHLIB */





