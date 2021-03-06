/******************************************************************************
 *
 *  Copyright (C) 2009-2013 Broadcom Corporation
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/

#include "bta_api.h"

#if( defined BLE_INCLUDED ) && (BLE_INCLUDED == TRUE)
#if( defined BTA_GATT_INCLUDED ) && (BTA_GATT_INCLUDED == TRUE)

#include <stdlib.h>
#include "gki.h"
#include "bd.h"
#include "bta_gatts_co.h"

/*****************************************************************************
**  Local type definitions
*****************************************************************************/

#define BTIF_GATTS_MAX_SRV_CHG_CLT_SIZE 50

typedef struct
{
    BOOLEAN             enable;
    UINT8               num_clients;
    tBTA_GATTS_SRV_CHG  srv_chg[BTIF_GATTS_MAX_SRV_CHG_CLT_SIZE];
} __attribute__((packed)) btif_gatts_srv_chg_cb_t;

/*****************************************************************************
**  Static variables
*****************************************************************************/

static btif_gatts_srv_chg_cb_t btif_gatts_srv_chg_cb;

/*****************************************************************************
**  Static functions
*****************************************************************************/

static void btif_gatts_check_init(void)
{
    btif_gatts_srv_chg_cb_t *p_cb= &btif_gatts_srv_chg_cb;

    if (!p_cb->enable)
    {
       memset(p_cb, 0, sizeof(btif_gatts_srv_chg_cb_t));
       p_cb->enable = TRUE;
    }
}

static BOOLEAN btif_gatts_srv_chg(tBTA_GATTS_SRV_CHG_CMD cmd,
                                  tBTA_GATTS_SRV_CHG_REQ *p_req,
                                  tBTA_GATTS_SRV_CHG_RSP *p_rsp)
{
    BOOLEAN status = TRUE;
    BOOLEAN found = FALSE;
    UINT8   i, j, idx, last_idx;
    btif_gatts_srv_chg_cb_t *p_cb = &btif_gatts_srv_chg_cb;
    BTIF_TRACE_DEBUG2("%s, command: %d", __FUNCTION__, cmd);
    btif_gatts_check_init();

    switch (cmd)
    {
        case BTA_GATTS_SRV_CHG_CMD_ADD_CLIENT:

            if (p_cb->num_clients < BTIF_GATTS_MAX_SRV_CHG_CLT_SIZE)
            {
                memcpy(&p_cb->srv_chg[p_cb->num_clients], &p_req->srv_chg, sizeof(tBTA_GATTS_SRV_CHG));
                p_cb->num_clients++;
            } else {
                status = FALSE;
            }
            break;

        case BTA_GATTS_SRV_CHG_CMD_UPDATE_CLIENT:

            for (i=0; i != p_cb->num_clients; ++i)
            {
                if (!memcmp(p_cb->srv_chg[i].bda, p_req->srv_chg.bda, sizeof(BD_ADDR)))
                {
                    found = TRUE;
                    memcpy(&p_cb->srv_chg[i], &p_req->srv_chg, sizeof(tBTA_GATTS_SRV_CHG));
                    break;
                }
            }

            if (!found)
                status = FALSE;
            break;

        case BTA_GATTS_SRV_CHG_CMD_REMOVE_CLIENT:

            for (i=0; i != p_cb->num_clients; ++i)
            {
                if (!memcmp(p_cb->srv_chg[i].bda, p_req->srv_chg.bda, sizeof(BD_ADDR)))
                {
                    found = TRUE;
                    last_idx = p_cb->num_clients - 1;

                    if (i != last_idx )
                    {
                        /* Update the array so there is no gap */
                        for (j=i; j != last_idx; ++j )
                        {
                            memcpy(&p_cb->srv_chg[j], &p_cb->srv_chg[j+1], sizeof(tBTA_GATTS_SRV_CHG));
                        }

                    }

                    /* Reset the last client and update num_clients */
                    memset(&p_cb->srv_chg[last_idx], 0, sizeof(tBTA_GATTS_SRV_CHG));
                    p_cb->num_clients--;
                    break;
                }
            }

            if (!found)
                status = FALSE;
            break;

        case BTA_GATTS_SRV_CHG_CMD_READ_NUM_CLENTS:
            p_rsp->num_clients = p_cb->num_clients;
            break;

        case BTA_GATTS_SRV_CHG_CMD_READ_CLENT:
            idx = p_req->client_read_index - 1;

            if (idx < p_cb->num_clients )
                memcpy(&p_rsp->srv_chg, &p_cb->srv_chg[idx], sizeof(tBTA_GATTS_SRV_CHG));
            else
                status = FALSE;
            break;

        default:
            status = FALSE;
            break;
    }

    return status;
}

/*****************************************************************************
**  Externally called functions
*****************************************************************************/

void btif_gatts_add_bonded_dev_from_nv(BD_ADDR bda)
{
    btif_gatts_srv_chg_cb_t *p_cb= &btif_gatts_srv_chg_cb;
    BOOLEAN                 found = FALSE;
    UINT8                   i;

    btif_gatts_check_init();

    for (i=0; i != p_cb->num_clients; ++i)
    {
        if (!memcmp(p_cb->srv_chg[i].bda,  bda, sizeof(BD_ADDR)))
        {
            found = TRUE;
            break;
        }
    }

    if (!found)
    {
        if (p_cb->num_clients < BTIF_GATTS_MAX_SRV_CHG_CLT_SIZE)
        {
            bdcpy(p_cb->srv_chg[p_cb->num_clients].bda, bda);
            p_cb->srv_chg[p_cb->num_clients].srv_changed = TRUE;
            p_cb->num_clients++;
        }
    }
    BTIF_TRACE_DEBUG2("%s: num_clients: %d", __FUNCTION__, p_cb->num_clients);
}

/*****************************************************************************
**  Call-out functions
*****************************************************************************/

/*******************************************************************************
**
** Function         bta_gatts_co_update_handle_range
**
** Description      This callout function is executed by GATTS when a GATT server
**                  handle range ios to be added or removed.
**
** Parameter        is_add: true is to add a handle range; otherwise is to delete.
**                  p_hndl_range: handle range.
**
** Returns          void.
**
*******************************************************************************/
void bta_gatts_co_update_handle_range(BOOLEAN is_add, tBTA_GATTS_HNDL_RANGE *p_hndl_range)
{
}

/*******************************************************************************
**
** Function         bta_gatts_co_srv_chg
**
** Description      This call-out is to read/write/remove service change related
**                  informaiton. The request consists of the cmd and p_req and the
**                  response is returned in p_rsp
**
** Parameter        cmd - request command
**                  p_req - request paramters
**                  p_rsp - response data for the request
**
** Returns          TRUE - if the request is processed successfully and
**                         the response is returned in p_rsp.
**                  FASLE - if the request can not be processed
**
*******************************************************************************/
BOOLEAN bta_gatts_co_srv_chg(tBTA_GATTS_SRV_CHG_CMD cmd,
                             tBTA_GATTS_SRV_CHG_REQ *p_req,
                             tBTA_GATTS_SRV_CHG_RSP *p_rsp)
{
    return btif_gatts_srv_chg(cmd, p_req, p_rsp);
}

/*******************************************************************************
**
** Function         bta_gatts_co_load_handle_range
**
** Description      This callout function is executed by GATTS when a GATT server
**                  handle range is requested to be loaded from NV.
**
** Parameter
**
** Returns          void.
**
*******************************************************************************/
BOOLEAN bta_gatts_co_load_handle_range(UINT8 index,
                                       tBTA_GATTS_HNDL_RANGE *p_handle_range)
{
   return FALSE;
}
#endif
#endif
