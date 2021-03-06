/*
**  Copyright (C) 2005-2006 Ingres Corporation. All Rights Reserved.		       
*/

/*
**    Source   : rvrevent.cpp, Implementation file
**    Project  : CA-OpenIngres/Replicator.
**    Author   : UK Sotheavut
**    Purpose  : Page of a Replication Server Item  (Raise Event)
**
** History:
**
** xx-Dec-1997 (uk$so01)
**    Created
**
*/

#include "stdafx.h"
#include "rcdepend.h"
#include "rvrevent.h"
#include "repitem.h"
#include "ipmprdta.h"
#include "ipmframe.h"
#include "ipmdoc.h"
#include "ipmdml.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


/////////////////////////////////////////////////////////////////////////////
// CuDlgReplicationServerPageRaiseEvent dialog


CuDlgReplicationServerPageRaiseEvent::CuDlgReplicationServerPageRaiseEvent(CWnd* pParent /*=NULL*/)
	: CDialog(CuDlgReplicationServerPageRaiseEvent::IDD, pParent)
{
	//{{AFX_DATA_INIT(CuDlgReplicationServerPageRaiseEvent)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}


void CuDlgReplicationServerPageRaiseEvent::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CuDlgReplicationServerPageRaiseEvent)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
}

void CuDlgReplicationServerPageRaiseEvent::Cleanup()
{
	int i, nCount = m_cListCtrl.GetItemCount();
	CaReplicationRaiseDbevent* pItem = NULL;

	for (i=0; i<nCount; i++)
	{
		pItem = (CaReplicationRaiseDbevent*)m_cListCtrl.GetItemData (i);
		if (pItem)
			delete pItem;
	}
}

void CuDlgReplicationServerPageRaiseEvent::AddItem (CaReplicationRaiseDbevent* pItem)
{
	int nCount = m_cListCtrl.GetItemCount();
	int nIndex = m_cListCtrl.InsertItem (nCount, pItem->GetAction()); // "Action"
	if (nIndex != -1)
	{
		m_cListCtrl.SetItemText (nIndex, 1, pItem->GetEvent());       // "Event"
		m_cListCtrl.SetItemText (nIndex, 2, pItem->GetServerFlag());  // "Server Flag"
		m_cListCtrl.SetItemData (nIndex, (DWORD)pItem);
	}
}


BEGIN_MESSAGE_MAP(CuDlgReplicationServerPageRaiseEvent, CDialog)
	//{{AFX_MSG_MAP(CuDlgReplicationServerPageRaiseEvent)
	ON_WM_SIZE()
	ON_BN_CLICKED(IDC_BUTTON1, OnButtonRaiseEvent)
	//}}AFX_MSG_MAP
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_LIST1, OnItemchangedList1)
	ON_WM_DESTROY()
	ON_MESSAGE (WM_USER_IPMPAGE_UPDATEING, OnUpdateData)
	ON_MESSAGE (WM_USER_IPMPAGE_LOADING,   OnLoad)
	ON_MESSAGE (WM_USER_IPMPAGE_GETDATA,   OnGetData)
	ON_MESSAGE (WMUSRMSG_CHANGE_SETTING,   OnPropertiesChange)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CuDlgReplicationServerPageRaiseEvent message handlers

void CuDlgReplicationServerPageRaiseEvent::OnItemchangedList1(NMHDR* pNMHDR, LRESULT* pResult)
{
	NM_LISTVIEW* pNMListView = (NM_LISTVIEW*)pNMHDR;
	EnableButtons();
	*pResult = 0;
}


void CuDlgReplicationServerPageRaiseEvent::OnDestroy() 
{
	Cleanup();
	CDialog::OnDestroy();
}


void CuDlgReplicationServerPageRaiseEvent::PostNcDestroy() 
{
	delete this;
	CDialog::PostNcDestroy();
}

BOOL CuDlgReplicationServerPageRaiseEvent::OnInitDialog() 
{
	CDialog::OnInitDialog();
	VERIFY (m_cListCtrl.SubclassDlgItem (IDC_LIST1, this));
	//
	// Initalize the Column Header of CListCtrl (CuListCtrl)
	// If modify this constant and the column width, make sure do not forget
	// to port to the OnLoad() and OnGetData() members.
	const int LAYOUT_NUMBER = 3;
	LSCTRLHEADERPARAMS2 lsp[LAYOUT_NUMBER] =
	{
		{IDS_TC_ACTION,  300,  LVCFMT_LEFT,FALSE},
		{IDS_TC_EVENT,    80,  LVCFMT_LEFT,FALSE},
		{IDS_TC_SVR_FLAG, 80,  LVCFMT_LEFT,FALSE}
	};
	InitializeHeader2(&m_cListCtrl, LVCF_FMT|LVCF_SUBITEM|LVCF_TEXT|LVCF_WIDTH, lsp, LAYOUT_NUMBER);

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CuDlgReplicationServerPageRaiseEvent::OnSize(UINT nType, int cx, int cy) 
{
	CDialog::OnSize(nType, cx, cy);
	if (!IsWindow (m_cListCtrl.m_hWnd))
		return;
	CRect rDlg, r;
	GetClientRect (rDlg);
	m_cListCtrl.GetWindowRect (r);
	ScreenToClient (r);
	r.right  = rDlg.right  - r.left;
	r.bottom = rDlg.bottom - r.left;
	m_cListCtrl.MoveWindow (r);
}

void CuDlgReplicationServerPageRaiseEvent::EnableButtons()
{
	BOOL bRaiseEventEnable = (m_cListCtrl.GetSelectedCount() == 1)? TRUE: FALSE;
	GetDlgItem(IDC_BUTTON1)->EnableWindow (bRaiseEventEnable);
}


void CuDlgReplicationServerPageRaiseEvent::OnButtonRaiseEvent() 
{
	
	int i, nSel = -1, nCount = m_cListCtrl.GetItemCount();
	for (i=0; i<nCount; i++)
	{
		if (m_cListCtrl.GetItemState(i, LVIS_SELECTED) & LVIS_SELECTED)
		{
			nSel = i;
			break;
		}
	}
	if (nSel == -1)
		return;
	CaReplicationRaiseDbevent* pItem = (CaReplicationRaiseDbevent*)m_cListCtrl.GetItemData (nSel);
	
	CString SvrFlag = pItem->GetServerFlag();
	int iret;
	if(pItem->AskQuestion()) {
		iret = pItem->AskQuestion4FilledServerFlag(&SvrFlag);
		if (iret == IDCANCEL) {
			m_cListCtrl.SetFocus();
			EnableButtons();
			return;
		}
	}


	CdIpmDoc* pIpmDoc = NULL;
	CfIpmFrame* pIpmFrame = (CfIpmFrame*)GetParentFrame();
	ASSERT(pIpmFrame);
	if (pIpmFrame)
	{
		pIpmDoc = pIpmFrame->GetIpmDoc();
		ASSERT (pIpmDoc);
	}
	if (!pIpmDoc)
		return;

	IPM_SendRaisedEvents( pIpmDoc, m_pSvrDta, pItem);
	m_cListCtrl.SetFocus();
	EnableButtons();
}


LONG CuDlgReplicationServerPageRaiseEvent::OnUpdateData (WPARAM wParam, LPARAM lParam)
{
	CdIpmDoc* pDoc = (CdIpmDoc*)wParam;
	LPIPMUPDATEPARAMS pUps = (LPIPMUPDATEPARAMS)lParam;
	m_pSvrDta = (LPREPLICSERVERDATAMIN)pUps->pStruct;
	BOOL bResult = IPM_GetSvrPageEvents(pDoc,m_EventsList,m_pSvrDta);
	Cleanup();
	m_cListCtrl.DeleteAllItems();

	CaReplicationRaiseDbevent* pItem = NULL;
	while (!m_EventsList.IsEmpty())
	{
		pItem = m_EventsList.RemoveHead();
		AddItem (pItem);
	}
	return 0L;
}

LONG CuDlgReplicationServerPageRaiseEvent::OnLoad (WPARAM wParam, LPARAM lParam)
{
	LPCTSTR pClass = (LPCTSTR)wParam;
	ASSERT (lstrcmp (pClass, _T("CaReplicationServerDataPageRaiseEvent")) == 0);

	CaReplicationServerDataPageRaiseEvent* pData =  (CaReplicationServerDataPageRaiseEvent*)lParam;
	ASSERT (pData);
	if (!pData)
		return 0L;
	CaReplicRaiseDbeventList* pListEvent = &(pData->m_listEvent);
	CaReplicationRaiseDbevent* pItem;
	POSITION pos = pListEvent->GetHeadPosition();
	try
	{
		// For each column:
		const int LAYOUT_NUMBER = 3;
		for (int i=0; i<LAYOUT_NUMBER; i++)
			m_cListCtrl.SetColumnWidth(i, pData->m_cxHeader.GetAt(i));
		while (pos != NULL)
		{
			pItem = pListEvent->GetNext (pos);
			ASSERT (pItem);
			AddItem(pItem);
		}
		m_cListCtrl.SetScrollPos (SB_HORZ, pData->m_scrollPos.cx);
		m_cListCtrl.SetScrollPos (SB_VERT, pData->m_scrollPos.cy);
	}
	catch (CMemoryException* e)
	{
		theApp.OutOfMemoryMessage();
		e->Delete();
	}
	return 0L;
}

LONG CuDlgReplicationServerPageRaiseEvent::OnGetData (WPARAM wParam, LPARAM lParam)
{
	CaReplicationServerDataPageRaiseEvent* pData = NULL;
	LV_COLUMN lvcolumn;
	memset (&lvcolumn, 0, sizeof (lvcolumn));
	lvcolumn.mask = LVCF_FMT|LVCF_SUBITEM|LVCF_WIDTH;

	try
	{
		CaReplicationRaiseDbevent* pItem;
		pData = new CaReplicationServerDataPageRaiseEvent();
		int i, nCount = m_cListCtrl.GetItemCount();
		for (i=0; i<nCount; i++)
		{
			pItem = (CaReplicationRaiseDbevent*)m_cListCtrl.GetItemData (i);
			pData->m_listEvent.AddTail (pItem);
		}
		//
		// For each column
		const int LAYOUT_NUMBER = 3;
		int hWidth   [LAYOUT_NUMBER] = {100, 80, 60};
		for (i=0; i<LAYOUT_NUMBER; i++)
		{
			if (m_cListCtrl.GetColumn(i, &lvcolumn))
				pData->m_cxHeader.InsertAt (i, lvcolumn.cx);
			else
				pData->m_cxHeader.InsertAt (i, hWidth[i]);
		}
		pData->m_scrollPos = CSize (m_cListCtrl.GetScrollPos (SB_HORZ), m_cListCtrl.GetScrollPos (SB_VERT));
	}
	catch (CMemoryException* e)
	{
		theApp.OutOfMemoryMessage();
		e->Delete();
	}
	return (LRESULT)pData;
}

long CuDlgReplicationServerPageRaiseEvent::OnPropertiesChange(WPARAM wParam, LPARAM lParam)
{
	PropertyChange(&m_cListCtrl, wParam, lParam);
	return 0;
}
