/* 
** Copyright (c) 2004 Ingres Corporation
*/

# include       <compat.h>
# include       <cv.h>          /* 6-x_PC_80x86 */
# include       <er.h>
# include       <me.h>
# include       <cm.h>
# include       <st.h>
# include       <gl.h>
# include       <sl.h>
# include       <iicommon.h>
# include       <gca.h>
# include       <adf.h>
# include       <adp.h>
# include       <iicgca.h>
# include       <erlc.h>
# include       <erlq.h>
# include       <generr.h>
# include       <sqlstate.h>
#define CS_SID      SCALARP
# include       <raat.h>
# include       <raatint.h>
# include	<iisqlca.h>
# include       <iilibq.h>
# include	<erug.h>
/*
** Name: IIraat_record_put - add a new record using the RAAT API
**
** Description:
**	Formulate GCA message for RAAT record put command and send it to
**	the DBMS, then wait for reply and return status of operation.
**
** Inputs:
**	raat_cb		RAAT control block
**
** Outputs:
**	None.
**
** Returns:
**	STATUS		OK or FAIL
**
** History:
**	16-apr-95 (stephenb)
**	    First written.
**      8-may-95 (lewda02/thaju02)
**          Extract from api.qc.
**          Streamline GCA usage.
**          Change naming convention.
**	14-jul-95 (emmag)
**	    Cleaned up parameters passed to IIGCa_call.
**      11-sep-95 (shust01/thaju02)
**          Added blob support.
**      14-sep-95 (shust01/thaju02)
**          Use RAAT internal_buf instead of GCA buffer(IIlbqcb ...), since
**          we could be passing more than 1 GCA buffers worth.
**	5-oct-95 (thaju02)
**	    Modified the blob coupon information buffered and sent to 
**	    backend.
**      10-nov-95 (thaju02)
**          Added check to test for table open and check of secondary
**          index table.
**	08-jul-96 (toumi01)
**	    Modified to support axp.osf (64-bit Digital Unix).
**	    Most of these changes are to handle that fact that on this
**	    platform sizeof(PTR)=8, sizeof(long)=8, sizeof(i4)=4.
**	    MEcopy is used to avoid bus errors moving unaligned data.
**      06-mar-96 (thaju02)
**          Support of variable page size project. Modified references of
**          DB_MAXTUP to (table handle's tbl_pgsize - page header).
**	16-jul-1996 (sweeney)
**	    Add tracing.
**	11-feb-1997 (cohmi01)
**	    Handle errors, give back RAAT return codes. (b80665)
**	08-sep-1997 (somsa01)
**	    In case of "rollback" conditions, mark that the the table is closed.
**	18-Dec-97 (gordy)
**	    Libq session data now accessed via function call.
**	    Converted to GCA control block interface.
**      24-Mar-99 (hweho01)
**          Extended the changes dated 08-jul-96 by toumi01 for axp_osf
**          to ris_u64 (AIX 64-bit platform).
**	21-jan-1999 (hanch04)
**	    replace nat and longnat with i4
**	31-aug-2000 (hanch04)
**	    cross change to main
**	    replace nat and longnat with i4
**	12-Jan-2001 (hanje04)
**	    Add Alpha Linux (axp_lnx) to 64-bit MEcopy change.
**	13-oct-2001 (somsa01)
**	    Porting changes for NT_IA64.
**	02-oct-2003 (somsa01)
**	    Ported to NT_AMD64.
*/
STATUS
IIraat_record_put (RAAT_CB	*raat_cb)
{
    GCA_PARMLIST	gca_parm;
    GCA_SD_PARMS	*gca_snd;
    GCA_RV_PARMS	*gca_recv;
    GCA_IT_PARMS	*gca_int;
    GCA_Q_DATA		*query_data;
    i4			msg_size;
    STATUS		gca_stat;
    int			i;
    char		*buf_ptr;
    i4                  space_needed;
    IICGC_MSG           *dataptr;
    i4             *blobcnt;
    i4             *has_extension;
    STATUS		status;

    /* log any tracing information */
    IIraat_trace(RAAT_RECORD_PUT, raat_cb);

    /*
    ** Test for valid open table before we proceed.
    */
    if (raat_cb->table_handle == NULL ||
        !(raat_cb->table_handle->table_status & TABLE_OPEN))
    {
	raat_cb->err_code = E_UG00D0_RaatTblNotOpen;
        return (FAIL);
    }

    /*
    ** Test if table is a secondary index.
    */
    if (raat_cb->table_handle->table_info.tbl_id.db_tab_index)
    {
	raat_cb->err_code = E_UG00D1_RaatInvOperSecIdx;
	return(FAIL);
    }

    /*
    ** Fill out gca data area, for record put the format is:
    **
    ** i4		op_code
    ** i4		table_handle->table_rcb
    ** char		*record
    **
    */
    /* starting at DB_MAXTUP, for blob support */
    space_needed = 6 * sizeof(i4) + 
	raat_cb->table_handle->table_info.tbl_width +
	((raat_cb->table_handle->table_info.tbl_pgsize - RAAT_DEF_PGHDR) /
	sizeof(ADP_PERIPHERAL) * (2 * sizeof(i4)) + 
	(2 * sizeof(i4)));
	     
    /* make sure internal buffer is big enough to handle data */
    if  (raat_cb->internal_buf_size < space_needed)
    {
        status = allocate_big_buffer(raat_cb, space_needed);
        if (status != OK)
	{
	    raat_cb->err_code = S_UG0036_NoMemory;
	    return(FAIL);
	}
    }
			  
    dataptr = (IICGC_MSG *)raat_cb->internal_buf;
    query_data = (GCA_Q_DATA *)dataptr->cgc_data;
    query_data->gca_language_id = DB_NDMF;
    query_data->gca_query_modifier = 0;
    /*
    ** copy value to gca data buffer and incriment message size
    */
    buf_ptr = (char *)query_data->gca_qdata;
    *((i4 *)buf_ptr) = RAAT_RECORD_PUT;
    msg_size = sizeof(i4);

    /*
    ** copy data value (table rcb) to gca buffer and increment 
    ** message size
    */
#if defined(axp_osf) || defined(ris_u64) || defined(axp_lnx) || defined(NT_IA64) || defined(NT_AMD64)
    MEcopy(&raat_cb->table_handle->table_rcb, sizeof(PTR), buf_ptr + msg_size);
    msg_size += sizeof(PTR);
#else
    *((i4 *)(buf_ptr + msg_size)) = raat_cb->table_handle->table_rcb;
    msg_size += sizeof(i4);
#endif
    /*
    ** copy data value (record + length) to gca buffer and 
    ** increment msg size
    */
    *((i4 *)(buf_ptr + msg_size)) =
	raat_cb->table_handle->table_info.tbl_width;
    msg_size += sizeof(i4);

    /*
    ** blob support
    ** send to backend : table has extensions flag, number
    ** of coupons(blobs) in record, and array of datatypes and offsets.
    ** if no blobs in record, send has_extension flag(=0) only.
    */
    has_extension = ((i4 *)(buf_ptr + msg_size));
    msg_size += sizeof(i4);
    *has_extension = 0;
 
    if (raat_cb->table_handle->table_info.tbl_2_status_mask &
        RAAT_HAS_EXTENSIONS)
    {
        RAAT_ATT_ENTRY  *attptr = (raat_cb->table_handle->table_att);
 
        *has_extension = 1;
        blobcnt = ((i4 *)(buf_ptr + msg_size));
        msg_size += sizeof(i4);
        *blobcnt = 0;
        for (i = 0; i < raat_cb->table_handle->table_info.tbl_attr_count; i++)
        {
            if ((abs((attptr + i)->att_type) == RAAT_LVCH_TYPE) ||
                (abs((attptr + i)->att_type) == RAAT_LBYTE_TYPE))
            {
		if (((attptr + i)->att_type > 0) ||
		    (((attptr + i)->att_type < 0) && 
		    (*(raat_cb->record + (attptr + i)->att_offset +
		    sizeof(ADP_PERIPHERAL)) == NULL)))
		{
                    *((i4 *)(buf_ptr + msg_size)) = (attptr + i)->att_type;
                    msg_size += sizeof(i4);
                    *((i4 *)(buf_ptr + msg_size)) = (attptr + i)->att_offset;
                    msg_size += sizeof(i4);
                    (*blobcnt)++;
		}
            }
        }
    }

    MEcopy(raat_cb->record,
	raat_cb->table_handle->table_info.tbl_width + sizeof(i4), 
	buf_ptr + msg_size);
    msg_size += raat_cb->table_handle->table_info.tbl_width + sizeof(i4);

    /*
    ** send info to the DBMS
    */

    gca_snd = &gca_parm.gca_sd_parms;

    gca_snd->gca_association_id	= IILQaiAssocID();
    gca_snd->gca_message_type = GCA_QUERY;
    gca_snd->gca_buffer	= dataptr->cgc_buffer;
    gca_snd->gca_msg_length = msg_size + sizeof(query_data->gca_language_id) +
				  sizeof(query_data->gca_query_modifier);
    gca_snd->gca_end_of_data = TRUE;
    gca_snd->gca_descriptor = 0;
    gca_snd->gca_status	= E_GC0000_OK;

    IIGCa_cb_call( &IIgca_cb, GCA_SEND, &gca_parm, 
		   GCA_SYNC_FLAG, 0, IICGC_NOTIME, &gca_stat );

    if (CHECK_RAAT_GCA_RET(raat_cb, gca_stat, gca_snd))
    {
	/* Mark the table closed based upon "rollback" conditions */
	if ( (raat_cb->err_code == 196674)	/* deadlock */
	  || (raat_cb->err_code == 4705)	/* maxlocks reached */
	  || (raat_cb->err_code == 4706) )	/* force abort */
	    raat_cb->table_handle->table_status &= ~(TABLE_OPEN);
	return (FAIL);	/* err_code in RAAT_CB has been set by macro */
    }

    /*
    ** Wait for reply
    */
    gca_recv = &gca_parm.gca_rv_parms;

    gca_recv->gca_association_id = IILQaiAssocID();
    gca_recv->gca_flow_type_indicator = GCA_NORMAL;
    gca_recv->gca_buffer = dataptr->cgc_buffer;
    gca_recv->gca_b_length = dataptr->cgc_d_length;
    gca_recv->gca_descriptor = NULL;
    gca_recv->gca_status = E_GC0000_OK;

    IIGCa_cb_call( &IIgca_cb, GCA_RECEIVE, &gca_parm, 
		   GCA_SYNC_FLAG, 0, IICGC_NOTIME, &gca_stat );

    if (CHECK_RAAT_GCA_RET(raat_cb, gca_stat, gca_recv))
    {
	/* Mark the table closed based upon "rollback" conditions */
	if ( (raat_cb->err_code == 196674)	/* deadlock */
	  || (raat_cb->err_code == 4705)	/* maxlocks reached */
	  || (raat_cb->err_code == 4706) )	/* force abort */
	    raat_cb->table_handle->table_status &= ~(TABLE_OPEN);
	return (FAIL);	/* err_code in RAAT_CB has been set by macro */
    }

    /*
    ** Interpret results
    */
    gca_int = &gca_parm.gca_it_parms;

    gca_int->gca_buffer		= dataptr->cgc_buffer;
    gca_int->gca_message_type	= 0;
    gca_int->gca_data_area	= (char *)0;          /* Output */
    gca_int->gca_d_length	= 0;                  /* Output */
    gca_int->gca_end_of_data	= 0;                  /* Output */
    gca_int->gca_status		= E_GC0000_OK;        /* Output */

    IIGCa_cb_call( &IIgca_cb, GCA_INTERPRET, &gca_parm, 
		   GCA_SYNC_FLAG, 0, IICGC_NOTIME, &gca_stat );

    if (CHECK_RAAT_GCA_RET(raat_cb, gca_stat, gca_int))
    {
	/* Mark the table closed based upon "rollback" conditions */
	if ( (raat_cb->err_code == 196674)	/* deadlock */
	  || (raat_cb->err_code == 4705)	/* maxlocks reached */
	  || (raat_cb->err_code == 4706) )	/* force abort */
	    raat_cb->table_handle->table_status &= ~(TABLE_OPEN);
	return (FAIL);	/* err_code in RAAT_CB has been set by macro */
    }
	     
    if (CHECK_RAAT_GCA_RESPONSE(raat_cb, gca_int))
	return (FAIL);	/* err_code in RAAT_CB has been set by macro */

    buf_ptr = (char *)gca_int->gca_data_area;
#if defined(axp_osf) || defined(ris_u64) || defined(NT_IA64) || defined(NT_AMD64)
    raat_cb->err_code = *((i4 *)buf_ptr);
#else
    raat_cb->err_code = *((long *)buf_ptr);
#endif
    if (raat_cb->err_code)
    {
	/* Mark the table closed based upon "rollback" conditions */
	if ( (raat_cb->err_code == 196674)	/* deadlock */
	  || (raat_cb->err_code == 4705)	/* maxlocks reached */
	  || (raat_cb->err_code == 4706) )	/* force abort */
	    raat_cb->table_handle->table_status &= ~(TABLE_OPEN);
        return (FAIL);
    }
    buf_ptr += sizeof(i4);
#if defined(axp_osf) || defined(ris_u64) || defined(NT_IA64) || defined(NT_AMD64)
    raat_cb->recnum = *((i4 *)buf_ptr);
#else
    raat_cb->recnum = *((long *)buf_ptr);
#endif

    return (OK);
}
