/*
 * Copyright (c) 2009, Christian Ferrari
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the names of the copyright holders nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include <config.h>



#ifdef HAVE_ASSERT_H
# include <assert.h>
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#endif



#include <lixa_errors.h>
#include <lixa_trace.h>
#include <lixa_tx.h>
#include <lixa_xa.h>
#include <lixa_xml_msg.h>
#include <client_conn.h>
#include <client_config.h>
#include <client_status.h>



/* set module trace flag */
#ifdef LIXA_TRACE_MODULE
# undef LIXA_TRACE_MODULE
#endif /* LIXA_TRACE_MODULE */
#define LIXA_TRACE_MODULE   LIXA_TRACE_MOD_CLIENT_TX



void xid_create_new(XID *xid)
{
    uuid_t uuid_obj;
    char *gtrid, *bqual;
    
    xid->formatID = LIXA_XID_FORMAT_ID;
    xid->gtrid_length = 2 * sizeof(uuid_t);
    xid->bqual_length = sizeof(uuid_t);
    memset(xid->data, 0, XIDDATASIZE); /* this is not necessary, but... */
    uuid_generate_time(uuid_obj);
    memcpy(xid->data, uuid_obj, sizeof(uuid_t));
    uuid_generate_random(uuid_obj);
    memcpy(xid->data + sizeof(uuid_t), uuid_obj, sizeof(uuid_t));
    uuid_generate(uuid_obj);
    memcpy(xid->data + 2 * sizeof(uuid_t), uuid_obj, sizeof(uuid_t));    
#ifdef _TRACE
    gtrid = xid_get_gtrid_ascii(xid);
    bqual = xid_get_bqual_ascii(xid);
    if (NULL != gtrid && NULL != bqual)
        LIXA_TRACE(("xid_create_new: gtrid='%s'; bqual='%s'\n", gtrid, bqual));
    if (NULL != bqual) free(bqual);
    if (NULL != gtrid) free(gtrid);
#endif /* _TRACE */
}



char *xid_get_gtrid_ascii(const XID *xid)
{
    char *gtrid;    
    if (NULL == (gtrid = (char *)malloc(2*(2*sizeof(uuid_t)+4)+2)))
        return NULL;
    uuid_unparse((unsigned char *)xid->data, gtrid);
    gtrid[2*sizeof(uuid_t)+4] = '+';
    uuid_unparse((unsigned char *)xid->data + sizeof(uuid_t),
                 gtrid + 2 * sizeof(uuid_t) + 5);
    return gtrid;
}



char *xid_get_bqual_ascii(const XID *xid)
{
    char *bqual;
    if (NULL == (bqual = (char *)malloc(2*sizeof(uuid_t)+5)))
        return NULL;
    uuid_unparse((unsigned char *)xid->data + 2 * sizeof(uuid_t), bqual);
    return bqual;
    
}



int lixa_tx_open(int *txrc)
{
    enum Exception { CLIENT_STATUS_COLL_GET_CS_ERROR
                     , CLIENT_STATUS_COLL_REGISTER_ERROR
                     , CLIENT_CONFIG_ERROR
                     , CLIENT_CONFIG_LOAD_SWITCH_ERROR
                     , CLIENT_CONNECT_ERROR
                     , COLL_GET_CS_ERROR
                     , LIXA_XA_OPEN_ERROR
                     , ALREADY_OPENED
                     , NONE } excp;
    int ret_cod = LIXA_RC_INTERNAL_ERROR;
    *txrc = TX_FAIL;

    LIXA_TRACE(("lixa_tx_open\n"));    
    TRY {
        int txstate, pos = 0;
        client_status_t *cs;
        
        /* check if the thread is already registered and
         * retrieve a reference to the status of the current thread */
        ret_cod = client_status_coll_get_cs(&global_csc, &cs);
        switch (ret_cod) {
            case LIXA_RC_OK: /* already registered, nothing to do */
                break;
            case LIXA_RC_OBJ_NOT_FOUND: /* first time, it must be registered */
                /* register this thread in library status */
                if (LIXA_RC_OK != (ret_cod = client_status_coll_register(
                                       &global_csc, &pos)))
                    THROW(CLIENT_STATUS_COLL_REGISTER_ERROR);
                cs = client_status_coll_get_status(&global_csc, pos);
                break;
            default:
                THROW(CLIENT_STATUS_COLL_GET_CS_ERROR);
        }
        
        /* check TX state (see Table 7-1) */
        txstate = client_status_get_txstate(cs);
        if (txstate == TX_STATE_S0) {
            if (LIXA_RC_OK != (ret_cod = client_config(&global_ccc)))
                THROW(CLIENT_CONFIG_ERROR);
            if (LIXA_RC_OK != (ret_cod =
                               client_config_load_switch(&global_ccc)))
                THROW(CLIENT_CONFIG_LOAD_SWITCH_ERROR);        
            if (LIXA_RC_OK != (ret_cod =
                               client_connect(&global_csc, &global_ccc)))
                THROW(CLIENT_CONNECT_ERROR);

            /* the real logic must be put here */
            if (LIXA_RC_OK != (ret_cod = lixa_xa_open(cs)))
                THROW(LIXA_XA_OPEN_ERROR);
            /* @@@ some logic mapping XA -> TX return code should put here */
            
            /* set new state after RMs are open... */
            client_status_set_txstate(cs, TX_STATE_S1);
        } else { /* already opened, nothing to do */
            LIXA_TRACE(("lixa_tx_open: already opened (txstate = %d), "
                        "bypassing...\n", txstate));    
            THROW(ALREADY_OPENED);
        }
        
        THROW(NONE);
    } CATCH {
        switch (excp) {
            case CLIENT_STATUS_COLL_GET_CS_ERROR:
            case CLIENT_STATUS_COLL_REGISTER_ERROR:
                break;
            case CLIENT_CONFIG_ERROR:
            case CLIENT_CONFIG_LOAD_SWITCH_ERROR:
                break;
            case CLIENT_CONNECT_ERROR:
                *txrc = TX_ERROR;
                break;
            case COLL_GET_CS_ERROR:
            case LIXA_XA_OPEN_ERROR:
                break;
            case ALREADY_OPENED:
                ret_cod = LIXA_RC_BYPASSED_OPERATION;
                *txrc = TX_OK;
                break;
            case NONE:
                *txrc = TX_OK;
                ret_cod = LIXA_RC_OK;
                break;
            default:
                ret_cod = LIXA_RC_INTERNAL_ERROR;
        } /* switch (excp) */
    } /* TRY-CATCH */
    LIXA_TRACE(("lixa_tx_open/excp=%d/"
                "ret_cod=%d/errno=%d\n", excp, ret_cod, errno));
    assert(ret_cod == TX_OK);
    return ret_cod;
}



int lixa_tx_close(int *txrc)
{
    enum Exception { COLL_GET_CS_ERROR
                     , CLIENT_DISCONNECT_ERROR
                     , CLIENT_CONFIG_UNLOAD_SWITCH_ERROR
                     , PROTOCOL_ERROR
                     , NONE } excp;
    int ret_cod = LIXA_RC_INTERNAL_ERROR;
    *txrc = TX_FAIL;
    
    LIXA_TRACE(("lixa_tx_close\n"));
    TRY {
        int txstate;
        client_status_t *cs;
        
        /* retrieve a read-only copy of the thread status */
        ret_cod = client_status_coll_get_cs(&global_csc, &cs);
        switch (ret_cod) {
            case LIXA_RC_OK: /* nothing to do */
                break;
            case LIXA_RC_OBJ_NOT_FOUND:
                *txrc = TX_OK;
                /* break intentionally missed */
            default:
                THROW(COLL_GET_CS_ERROR);
        }

        /* check TX state (see Table 7-1) */
        txstate = client_status_get_txstate(cs);

        switch (txstate) {
            case TX_STATE_S0:
            case TX_STATE_S1:
            case TX_STATE_S2:
                break;
            default:
                THROW(PROTOCOL_ERROR);
        }
        
        if (LIXA_RC_OK != (ret_cod = client_disconnect(&global_csc)))
            THROW(CLIENT_DISCONNECT_ERROR);

        if (LIXA_RC_OK != (ret_cod = client_config_unload_switch(&global_ccc)))
            THROW(CLIENT_CONFIG_UNLOAD_SWITCH_ERROR);

        /* update the TX state, now TX_STATE_S0 */
        client_status_set_txstate(cs, TX_STATE_S0);
        
        THROW(NONE);
    } CATCH {
        switch (excp) {
            case COLL_GET_CS_ERROR:
                break;
            case CLIENT_DISCONNECT_ERROR:
            case CLIENT_CONFIG_UNLOAD_SWITCH_ERROR:
                *txrc = TX_ERROR;
                break;
            case PROTOCOL_ERROR:
                *txrc = TX_PROTOCOL_ERROR;
                ret_cod = LIXA_RC_PROTOCOL_ERROR;
                break;
            case NONE:
                *txrc = TX_OK;
                ret_cod = LIXA_RC_OK;
                break;
            default:
                ret_cod = LIXA_RC_INTERNAL_ERROR;
                break;
        } /* switch (excp) */
    } /* TRY-CATCH */
    LIXA_TRACE(("lixa_tx_close/excp=%d/"
                "ret_cod=%d/errno=%d\n", excp, ret_cod, errno));
    assert(ret_cod == LIXA_RC_OK);
    return ret_cod;
}

