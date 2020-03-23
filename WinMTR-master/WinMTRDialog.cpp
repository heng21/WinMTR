//*****************************************************************************
// FILE:            WinMTRDialog.cpp
//
//
//*****************************************************************************

#include "WinMTRGlobal.h"
#include "WinMTRDialog.h"
#include "WinMTROptions.h"
#include "WinMTRHelp.h"
#include "WinMTRProperties.h"
#include "WinMTRNet.h"
#include <iostream>
#include <sstream>
#include "afxlinkctrl.h"
#include<time.h>

#define TRACE_MSG(msg)										\
	{														\
	std::ostringstream dbg_msg(std::ostringstream::out);	\
	dbg_msg << msg << std::endl;							\
	OutputDebugString(dbg_msg.str().c_str());				\
	}

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static	 char THIS_FILE[] = __FILE__;
#endif

void PingThread(void *p);

//*****************************************************************************
// BEGIN_MESSAGE_MAP
//
// 
//*****************************************************************************
BEGIN_MESSAGE_MAP(WinMTRDialog, CDialog)
	ON_WM_PAINT()
	ON_WM_SIZE()
	ON_WM_SIZING()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(ID_RESTART, OnRestart)
	ON_BN_CLICKED(ID_OPTIONS, OnOptions)
	ON_BN_CLICKED(ID_CTTC, OnCTTC)
	ON_BN_CLICKED(ID_CHTC, OnCHTC)
	ON_BN_CLICKED(ID_EXPT, OnEXPT)
	ON_BN_CLICKED(ID_EXPH, OnEXPH)
	ON_NOTIFY(NM_DBLCLK, IDC_LIST_MTR, OnDblclkList)
	ON_CBN_SELCHANGE(IDC_COMBO_HOST, &WinMTRDialog::OnCbnSelchangeComboHost)
	ON_CBN_SELENDOK(IDC_COMBO_HOST, &WinMTRDialog::OnCbnSelendokComboHost)
	ON_CBN_CLOSEUP(IDC_COMBO_HOST, &WinMTRDialog::OnCbnCloseupComboHost)
	ON_WM_TIMER()
	ON_WM_CLOSE()
	ON_BN_CLICKED(IDCANCEL, &WinMTRDialog::OnBnClickedCancel)
	ON_NOTIFY(NM_RCLICK, IDC_LIST_MTR, &WinMTRDialog::OnRclickListMtr)
	ON_COMMAND(ID_MENU_COPYIP, &WinMTRDialog::OnMenuCopyip)
	ON_COMMAND(ID_MENU_WHOIS, &WinMTRDialog::OnMenuWhois)
	ON_COMMAND(ID_MENU_IPIP, &WinMTRDialog::OnMenuIpip)
	ON_WM_HELPINFO()
END_MESSAGE_MAP()


//*****************************************************************************
// WinMTRDialog::WinMTRDialog
//
// 
//*****************************************************************************
WinMTRDialog::WinMTRDialog(CWnd* pParent) 
			: CDialog(WinMTRDialog::IDD, pParent),
			state(IDLE),
			transition(IDLE_TO_IDLE)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
	m_autostart = 0;
	useDNS = DEFAULT_DNS;
	interval = DEFAULT_INTERVAL;
	pingsize = DEFAULT_PING_SIZE;
	maxLRU = DEFAULT_MAX_LRU;
	nrLRU = 0;
	countsize = DEFAULT_COUNTSIZE;

	hasIntervalFromCmdLine = false;
	hasPingsizeFromCmdLine = false;
	hasMaxLRUFromCmdLine = false;
	hasCountsizeFromCmdLine = false;
	hasUseDNSFromCmdLine = false;

	traceThreadMutex = CreateMutex(NULL, FALSE, NULL);
	wmtrnet = new WinMTRNet(this);
}

WinMTRDialog::~WinMTRDialog()
{
	delete wmtrnet;
	CloseHandle(traceThreadMutex);
}

//*****************************************************************************
// WinMTRDialog::DoDataExchange
//
// 
//*****************************************************************************
void WinMTRDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, ID_OPTIONS, m_buttonOptions);
	DDX_Control(pDX, IDCANCEL, m_buttonExit);
	DDX_Control(pDX, ID_RESTART, m_buttonStart);
	DDX_Control(pDX, IDC_COMBO_HOST, m_comboHost);
	DDX_Control(pDX, IDC_LIST_MTR, m_listMTR);
	DDX_Control(pDX, IDC_STATICS, m_staticS);
	DDX_Control(pDX, IDC_STATICJ, m_staticJ);
	DDX_Control(pDX, ID_EXPH, m_buttonExpH);
	DDX_Control(pDX, ID_EXPT, m_buttonExpT);
}


//*****************************************************************************
// WinMTRDialog::OnInitDialog
//
// 
//*****************************************************************************
BOOL WinMTRDialog::OnInitDialog()
{
	CDialog::OnInitDialog();

	#ifndef  _WIN64
	char caption[] = {"WinMTR v1.0.0 32 bit"};
	#else
	char caption[] = {"WinMTR v1.0.0 64 bit"};
	#endif

	SetTimer(1, WINMTR_DIALOG_TIMER, NULL);
	SetWindowText(caption);

	SetIcon(m_hIcon, TRUE);			
	SetIcon(m_hIcon, FALSE);
	
	if(!statusBar.Create( this ))
		AfxMessageBox("Error creating status bar");
	statusBar.GetStatusBarCtrl().SetMinHeight(23);
		
	UINT sbi[1];
	sbi[0] = IDS_STRING_SB_NAME;	
	statusBar.SetIndicators( sbi,1);
	statusBar.SetPaneInfo(0, statusBar.GetItemID(0),SBPS_STRETCH, NULL );

	m_listMTR.SetExtendedStyle(m_listMTR.GetExtendedStyle() | LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);

	for(int i = 0; i< MTR_NR_COLS; i++)
		m_listMTR.InsertColumn(i, MTR_COLS[i], LVCFMT_LEFT, MTR_COL_LENGTH[i] , -1);
   
	m_comboHost.SetFocus();

	// We need to resize the dialog to make room for control bars.
	// First, figure out how big the control bars are.
	CRect rcClientStart;
	CRect rcClientNow;
	GetClientRect(rcClientStart);
	RepositionBars(AFX_IDW_CONTROLBAR_FIRST, AFX_IDW_CONTROLBAR_LAST,
				   0, reposQuery, rcClientNow);

	// Now move all the controls so they are in the same relative
	// position within the remaining client area as they would be
	// with no control bars.
	CPoint ptOffset(rcClientNow.left - rcClientStart.left,
					rcClientNow.top - rcClientStart.top);

	CRect  rcChild;
	CWnd* pwndChild = GetWindow(GW_CHILD);
	while (pwndChild)
	{
		pwndChild->GetWindowRect(rcChild);
		ScreenToClient(rcChild);
		rcChild.OffsetRect(ptOffset);
		pwndChild->MoveWindow(rcChild, FALSE);
		pwndChild = pwndChild->GetNextWindow();
	}

	SetWindowPos(&wndNoTopMost, 0, 0, 895, 590, SWP_NOMOVE);
	CenterWindow(GetDesktopWindow());

	// Adjust the dialog window dimensions
	CRect rcWindow;
	GetWindowRect(rcWindow);
	rcWindow.right += rcClientStart.Width() - rcClientNow.Width();
	rcWindow.bottom += rcClientStart.Height() - rcClientNow.Height();
	MoveWindow(rcWindow, FALSE);

	// And position the control bars
	RepositionBars(AFX_IDW_CONTROLBAR_FIRST, AFX_IDW_CONTROLBAR_LAST, 0);

	InitRegistry();

	if (m_autostart) {
		m_comboHost.SetWindowText(msz_defaulthostname);
		OnRestart();
	}

	return FALSE;
}

//*****************************************************************************
// WinMTRDialog::InitRegistry
//
// 
//*****************************************************************************
BOOL WinMTRDialog::InitRegistry()
{
	HKEY hKey, hKey_v;
	DWORD res, tmp_dword, value_size;
	LONG r;

	r = RegCreateKeyEx(	HKEY_CURRENT_USER, 
					"Software", 
					0, 
					NULL,
					REG_OPTION_NON_VOLATILE,
					KEY_ALL_ACCESS,
					NULL,
					&hKey,
					&res);
	if( r != ERROR_SUCCESS) 
		return FALSE;

	r = RegCreateKeyEx(	hKey, 
					"WinMTR", 
					0, 
					NULL,
					REG_OPTION_NON_VOLATILE,
					KEY_ALL_ACCESS,
					NULL,
					&hKey,
					&res);
	if( r != ERROR_SUCCESS) 
		return FALSE;



	r = RegCreateKeyEx(	hKey, 
					"Config", 
					0, 
					NULL,
					REG_OPTION_NON_VOLATILE,
					KEY_ALL_ACCESS,
					NULL,
					&hKey_v,
					&res);
	if( r != ERROR_SUCCESS) 
		return FALSE;

	if(RegQueryValueEx(hKey_v, "PingSize", 0, NULL, (unsigned char *)&tmp_dword, &value_size) != ERROR_SUCCESS) {
		tmp_dword = pingsize;
		RegSetValueEx(hKey_v,"PingSize", 0, REG_DWORD, (const unsigned char *)&tmp_dword, sizeof(DWORD));
	} else {
		if(!hasPingsizeFromCmdLine) pingsize = tmp_dword;
	}
	
	if(RegQueryValueEx(hKey_v, "MaxLRU", 0, NULL, (unsigned char *)&tmp_dword, &value_size) != ERROR_SUCCESS) {
		tmp_dword = maxLRU;
		RegSetValueEx(hKey_v,"MaxLRU", 0, REG_DWORD, (const unsigned char *)&tmp_dword, sizeof(DWORD));
	} else {
		if(!hasMaxLRUFromCmdLine) maxLRU = tmp_dword;
	}
	if (RegQueryValueEx(hKey_v, "Countsize", 0, NULL, (unsigned char *)&tmp_dword, &value_size) != ERROR_SUCCESS) {
		tmp_dword = countsize;
		RegSetValueEx(hKey_v, "Countsize", 0, REG_DWORD, (const unsigned char *)&tmp_dword, sizeof(DWORD));
	}else {
		if (!hasCountsizeFromCmdLine) countsize = tmp_dword;
	}
	if(RegQueryValueEx(hKey_v, "UseDNS", 0, NULL, (unsigned char *)&tmp_dword, &value_size) != ERROR_SUCCESS) {
		tmp_dword = useDNS ? 1 : 0;
		RegSetValueEx(hKey_v,"UseDNS", 0, REG_DWORD, (const unsigned char *)&tmp_dword, sizeof(DWORD));
	} else {
		if(!hasUseDNSFromCmdLine) useDNS = (BOOL)tmp_dword;
	}

	if(RegQueryValueEx(hKey_v, "Interval", 0, NULL, (unsigned char *)&tmp_dword, &value_size) != ERROR_SUCCESS) {
		tmp_dword = (DWORD)(interval * 1000);
		RegSetValueEx(hKey_v,"Interval", 0, REG_DWORD, (const unsigned char *)&tmp_dword, sizeof(DWORD));
	} else {
		if(!hasIntervalFromCmdLine) interval = (float)tmp_dword / 1000.0;
	}

	r = RegCreateKeyEx(	hKey, 
					"LRU", 
					0, 
					NULL,
					REG_OPTION_NON_VOLATILE,
					KEY_ALL_ACCESS,
					NULL,
					&hKey_v,
					&res);
	if( r != ERROR_SUCCESS) 
		return FALSE;
	if(RegQueryValueEx(hKey_v, "NrLRU", 0, NULL, (unsigned char *)&tmp_dword, &value_size) != ERROR_SUCCESS) {
		tmp_dword = nrLRU;
		RegSetValueEx(hKey_v,"NrLRU", 0, REG_DWORD, (const unsigned char *)&tmp_dword, sizeof(DWORD));
	} else {
		char key_name[20];
		unsigned char str_host[255];
		nrLRU = tmp_dword;
		for(int i=0;i<maxLRU;i++) {
			sprintf(key_name,"Host%d", i+1);
			if((r = RegQueryValueEx(hKey_v, key_name, 0, NULL, NULL, &value_size)) == ERROR_SUCCESS) {
				RegQueryValueEx(hKey_v, key_name, 0, NULL, str_host, &value_size);
				str_host[value_size]='\0';
				m_comboHost.AddString((CString)str_host);
			}
		}
	}
	m_comboHost.AddString(CString((LPCSTR)IDS_STRING_CLEAR_HISTORY));
	RegCloseKey(hKey_v);
	RegCloseKey(hKey);
	return TRUE;
}


//*****************************************************************************
// WinMTRDialog::OnSizing
//
// 
//*****************************************************************************
void WinMTRDialog::OnSizing(UINT fwSide, LPRECT pRect) 
{
	CDialog::OnSizing(fwSide, pRect);

	int iWidth = (pRect->right)-(pRect->left);
	int iHeight = (pRect->bottom)-(pRect->top);

	if (iWidth < 600)
		pRect->right = pRect->left + 600;
	if (iHeight <250)
		pRect->bottom = pRect->top + 250;
}


//*****************************************************************************
// WinMTRDialog::OnSize
//
// 
//*****************************************************************************
void WinMTRDialog::OnSize(UINT nType, int cx, int cy)
{
	CDialog::OnSize(nType, cx, cy);
	CRect r;
	GetClientRect(&r);
	CRect lb;
	
	if (::IsWindow(m_staticS.m_hWnd)) {
		m_staticS.GetWindowRect(&lb);
		ScreenToClient(&lb);
		m_staticS.SetWindowPos(NULL, lb.TopLeft().x, lb.TopLeft().y, r.Width()-lb.TopLeft().x-10, lb.Height() , SWP_NOMOVE | SWP_NOZORDER);
	}

	if (::IsWindow(m_staticJ.m_hWnd)) {
		m_staticJ.GetWindowRect(&lb);
		ScreenToClient(&lb);
		m_staticJ.SetWindowPos(NULL, lb.TopLeft().x, lb.TopLeft().y, r.Width() - 21, lb.Height(), SWP_NOMOVE | SWP_NOZORDER);
	}

	if (::IsWindow(m_buttonExit.m_hWnd)) {
		m_buttonExit.GetWindowRect(&lb);
		ScreenToClient(&lb);
		m_buttonExit.SetWindowPos(NULL, r.Width() - lb.Width()-21, lb.TopLeft().y, lb.Width(), lb.Height() , SWP_NOSIZE | SWP_NOZORDER);
	}
	
	if (::IsWindow(m_buttonExpH.m_hWnd)) {
		m_buttonExpH.GetWindowRect(&lb);
		ScreenToClient(&lb);
		m_buttonExpH.SetWindowPos(NULL, r.Width() - lb.Width()-21, lb.TopLeft().y, lb.Width(), lb.Height() , SWP_NOSIZE | SWP_NOZORDER);
	}


	if (::IsWindow(m_listMTR.m_hWnd)) {
		m_listMTR.GetWindowRect(&lb);
		ScreenToClient(&lb);
		m_listMTR.SetWindowPos(NULL, lb.TopLeft().x, lb.TopLeft().y, r.Width() - 21, r.Height() - lb.top - 25, SWP_NOMOVE | SWP_NOZORDER);
	}

	RepositionBars(AFX_IDW_CONTROLBAR_FIRST, AFX_IDW_CONTROLBAR_LAST,
				   0, reposQuery, r);

	RepositionBars(AFX_IDW_CONTROLBAR_FIRST, AFX_IDW_CONTROLBAR_LAST, 0);

}


//*****************************************************************************
// WinMTRDialog::OnPaint
//
// 
//*****************************************************************************
void WinMTRDialog::OnPaint() 
{
	if (IsIconic())
	{
		CPaintDC dc(this);

		SendMessage(WM_ICONERASEBKGND, (WPARAM) dc.GetSafeHdc(), 0);

		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialog::OnPaint();
	}
}


//*****************************************************************************
// WinMTRDialog::OnQueryDragIcon
//
// 
//*****************************************************************************
HCURSOR WinMTRDialog::OnQueryDragIcon()
{
	return (HCURSOR) m_hIcon;
}


//*****************************************************************************
// WinMTRDialog::OnDblclkList
//
//*****************************************************************************
void WinMTRDialog::OnDblclkList(NMHDR* pNMHDR, LRESULT* pResult)
{
	*pResult = 0;
		

		POSITION pos = m_listMTR.GetFirstSelectedItemPosition();
		if (pos != NULL) {
			int nItem = m_listMTR.GetNextSelectedItem(pos);
			WinMTRProperties wmtrprop;

			if (wmtrnet->GetAddr(nItem) == 0) {
				strcpy(wmtrprop.comment, "No response.");

			}
			else {

				strcpy(wmtrprop.comment, "Host alive.");
			}

			wmtrnet->GetName(nItem, wmtrprop.host);
			wmtrnet->GetIP(nItem, wmtrprop.ip);
			wmtrprop.ping_avrg = (float)wmtrnet->GetAvg(nItem);
			wmtrprop.ping_last = (float)wmtrnet->GetLast(nItem);
			wmtrprop.ping_best = (float)wmtrnet->GetBest(nItem);
			wmtrprop.ping_worst = (float)wmtrnet->GetWorst(nItem);

			wmtrprop.pck_loss = wmtrnet->GetPercent(nItem);
			wmtrprop.pck_recv = wmtrnet->GetReturned(nItem);
			wmtrprop.pck_sent = wmtrnet->GetXmit(nItem);


			wmtrprop.DoModal();
		}
}


//*****************************************************************************
// WinMTRDialog::SetHostName
//
//*****************************************************************************
void WinMTRDialog::SetHostName(const char *host)
{
	m_autostart = 1;
	strncpy(msz_defaulthostname,host,1000);
}


//*****************************************************************************
// WinMTRDialog::SetPingSize
//
//*****************************************************************************
void WinMTRDialog::SetPingSize(int ps)
{
	pingsize = ps;
}

//*****************************************************************************
// WinMTRDialog::SetMaxLRU
//
//*****************************************************************************
void WinMTRDialog::SetMaxLRU(int mlru)
{
	maxLRU = mlru;
}


//*****************************************************************************
// WinMTRDialog::SetInterval
//
//*****************************************************************************
void WinMTRDialog::SetInterval(float i)
{
	interval = i;
}

//*****************************************************************************
// WinMTRDialog::SetCountsize
//
//*****************************************************************************
void WinMTRDialog::SetCountsize(int c)
{
	countsize = c;
}



//*****************************************************************************
// WinMTRDialog::SetUseDNS
//
//*****************************************************************************
void WinMTRDialog::SetUseDNS(BOOL udns)
{
	useDNS = udns;
}




//*****************************************************************************
// WinMTRDialog::OnRestart
//
// 
//*****************************************************************************
void WinMTRDialog::OnRestart() 
{
	// If clear history is selected, just clear the registry and listbox and return
	if(m_comboHost.GetCurSel() == m_comboHost.GetCount() - 1) {
		ClearHistory();
		return;
	}

	CString sHost;

	if(state == IDLE) {
		m_comboHost.GetWindowText(sHost);
		sHost.TrimLeft();
		sHost.TrimLeft();
      
		if(sHost.IsEmpty()) {
			AfxMessageBox("No host specified!");
			m_comboHost.SetFocus();
			return ;
		}
		m_listMTR.DeleteAllItems();
	}

	if(state == IDLE) {

		if(InitMTRNet()) {
			if(m_comboHost.FindString(-1, sHost) == CB_ERR) {
				m_comboHost.InsertString(m_comboHost.GetCount() - 1,sHost);

				HKEY hKey;
				DWORD tmp_dword;
				LONG r;
				char key_name[20];

				r = RegOpenKeyEx(	HKEY_CURRENT_USER, "Software", 0, KEY_ALL_ACCESS,&hKey);
				r = RegOpenKeyEx(	hKey, "WinMTR", 0, KEY_ALL_ACCESS, &hKey);
				r = RegOpenKeyEx(	hKey, "LRU", 0, KEY_ALL_ACCESS, &hKey);

				if(nrLRU >= maxLRU)
					nrLRU = 0;
				
				nrLRU++;
				sprintf(key_name, "Host%d", nrLRU);
				r = RegSetValueEx(hKey,key_name, 0, REG_SZ, (const unsigned char *)(LPCTSTR)sHost, strlen((LPCTSTR)sHost)+1);
				tmp_dword = nrLRU;
				r = RegSetValueEx(hKey,"NrLRU", 0, REG_DWORD, (const unsigned char *)&tmp_dword, sizeof(DWORD));
				RegCloseKey(hKey);
			}
			Transit(TRACING);
		}
	} else {
		Transit(STOPPING);
	}
}


//*****************************************************************************
// WinMTRDialog::OnOptions
//
// 
//*****************************************************************************
void WinMTRDialog::OnOptions() 
{
	WinMTROptions optDlg;

	optDlg.SetPingSize(pingsize);
	optDlg.SetInterval(interval);
	optDlg.SetMaxLRU(maxLRU);
	optDlg.SetCountsize(countsize);
	optDlg.SetUseDNS(useDNS);

	if(IDOK == optDlg.DoModal()) {

		pingsize = optDlg.GetPingSize();
		interval = optDlg.GetInterval();
		maxLRU = optDlg.GetMaxLRU();
		countsize = optDlg.GetCountsize();
		useDNS = optDlg.GetUseDNS();

		HKEY hKey;
		DWORD tmp_dword;
		LONG r;
		char key_name[20];

		r = RegOpenKeyEx(	HKEY_CURRENT_USER, "Software", 0, KEY_ALL_ACCESS,&hKey);
		r = RegOpenKeyEx(	hKey, "WinMTR", 0, KEY_ALL_ACCESS, &hKey);
		r = RegOpenKeyEx(	hKey, "Config", 0, KEY_ALL_ACCESS, &hKey);
		tmp_dword = pingsize;
		RegSetValueEx(hKey,"PingSize", 0, REG_DWORD, (const unsigned char *)&tmp_dword, sizeof(DWORD));
		tmp_dword = maxLRU;
		RegSetValueEx(hKey,"MaxLRU", 0, REG_DWORD, (const unsigned char *)&tmp_dword, sizeof(DWORD));
		tmp_dword = countsize;
		RegSetValueEx(hKey, "Countsize", 0, REG_DWORD, (const unsigned char *)&tmp_dword, sizeof(DWORD));
		tmp_dword = useDNS ? 1 : 0;
		RegSetValueEx(hKey,"UseDNS", 0, REG_DWORD, (const unsigned char *)&tmp_dword, sizeof(DWORD));
		tmp_dword = (DWORD)(interval * 1000);
		RegSetValueEx(hKey,"Interval", 0, REG_DWORD, (const unsigned char *)&tmp_dword, sizeof(DWORD));
		RegCloseKey(hKey);
		if(maxLRU<nrLRU) {
			r = RegOpenKeyEx(	HKEY_CURRENT_USER, "Software", 0, KEY_ALL_ACCESS,&hKey);
			r = RegOpenKeyEx(	hKey, "WinMTR", 0, KEY_ALL_ACCESS, &hKey);
			r = RegOpenKeyEx(	hKey, "LRU", 0, KEY_ALL_ACCESS, &hKey);

			for(int i = maxLRU; i<=nrLRU; i++) {
					sprintf(key_name, "Host%d", i);
					r = RegDeleteValue(hKey,key_name);
			}
			nrLRU = maxLRU;
			tmp_dword = nrLRU;
			r = RegSetValueEx(hKey,"NrLRU", 0, REG_DWORD, (const unsigned char *)&tmp_dword, sizeof(DWORD));
			RegCloseKey(hKey);
		}
	}
}


//*****************************************************************************
// WinMTRDialog::OnCTTC
//
// 
//*****************************************************************************
void WinMTRDialog::OnCTTC() 
{	
	char buf[255], t_buf[1000], f_buf[255*100];
	CString ti;

	int nh = wmtrnet->GetMax();
	
	
    sprintf(t_buf, "IP              Loss%% Sent Recv Best Avrg Worst Last\r\n");
    strcpy(f_buf, t_buf);
	for(int i=0;i <nh ; i++) {
		wmtrnet->GetIP(i, buf);
		
		sprintf(t_buf, "%-15s %4d %4d %4d %4d %4d %4d %4d\r\n" , 
					buf, wmtrnet->GetPercent(i),
					wmtrnet->GetXmit(i), wmtrnet->GetReturned(i), wmtrnet->GetBest(i),
					wmtrnet->GetAvg(i), wmtrnet->GetWorst(i), wmtrnet->GetLast(i));
		strcat(f_buf, t_buf);
	}	
	

	CString cs_tmp((LPCSTR)IDS_STRING_SB_NAME);
	strcat(f_buf, "   ");
	strcat(f_buf, (LPCTSTR)cs_tmp);

	CTime tm = CTime::GetCurrentTime();
	ti = tm.Format("  %d/%m/%Y %X\r\n");
	strcat(f_buf, ti);


	CString source(f_buf);

	HGLOBAL clipbuffer;
	char * buffer;
	
	OpenClipboard();
	EmptyClipboard();
	
	clipbuffer = GlobalAlloc(GMEM_DDESHARE, source.GetLength()+1);
	buffer = (char*)GlobalLock(clipbuffer);
	strcpy(buffer, LPCSTR(source));
	GlobalUnlock(clipbuffer);
	
	SetClipboardData(CF_TEXT,clipbuffer);
	CloseClipboard();
}


//*****************************************************************************
// WinMTRDialog::OnCHTC
//
// 
//*****************************************************************************
void WinMTRDialog::OnCHTC() 
{
	char buf[255], t_buf[1000], f_buf[255*100];
	CString ti;

	int nh = wmtrnet->GetMax();
	
	strcpy(f_buf, "<html><head><title>WinMTR Statistics</title></head><body bgcolor=\"white\">\r\n");
	sprintf(t_buf, "<center><h2>WinMTR statistics</h2></center>\r\n");
	CTime tm = CTime::GetCurrentTime();
	ti = tm.Format("<center><h3>%d/%m/%Y %X</h3></center>\r\n");

	strcat(t_buf, ti);
	strcat(f_buf, t_buf);
	
	sprintf(t_buf, "<p align=\"center\"> <table border=\"1\" align=\"center\">\r\n" ); 
	strcat(f_buf, t_buf);
	
	sprintf(t_buf, "<tr><td>#</td> <td>IP</td> <td>Loss%%</td> <td>Sent</td> <td>Recv</td> <td>Best</td> <td>Avrg</td> <td>Worst</td> <td>Last</td></tr>\r\n" ); 
	strcat(f_buf, t_buf);

	for(int i=0;i <nh ; i++) {
		wmtrnet->GetIP(i, buf);
		
		sprintf(t_buf, "<tr><td>%4d</td> <td>%s</td> <td>%4d</td> <td>%4d</td> <td>%4d</td> <td>%4d</td> <td>%4d</td> <td>%4d</td> <td>%4d</td></tr>\r\n" , 
					i,buf, wmtrnet->GetPercent(i),
					wmtrnet->GetXmit(i), wmtrnet->GetReturned(i), wmtrnet->GetBest(i),
					wmtrnet->GetAvg(i), wmtrnet->GetWorst(i), wmtrnet->GetLast(i));
		strcat(f_buf, t_buf);
	}	
	

	sprintf(t_buf, "</table></body></html>\r\n"); 
	strcat(f_buf, t_buf);

	CString source(f_buf);

	HGLOBAL clipbuffer;
	char * buffer;
	
	OpenClipboard();
	EmptyClipboard();
	
	clipbuffer = GlobalAlloc(GMEM_DDESHARE, source.GetLength()+1);
	buffer = (char*)GlobalLock(clipbuffer);
	strcpy(buffer, LPCSTR(source));
	GlobalUnlock(clipbuffer);
	
	SetClipboardData(CF_TEXT,clipbuffer);
	CloseClipboard();
}


//*****************************************************************************
// WinMTRDialog::OnEXPT
//
// 
//*****************************************************************************
void WinMTRDialog::OnEXPT() 
{	
	TCHAR BASED_CODE szFilter[] = _T("Text Files (*.txt)|*.txt|All Files (*.*)|*.*||");

	CFileDialog dlg(FALSE,
                   _T("TXT"),
                   NULL,
                   OFN_HIDEREADONLY | OFN_EXPLORER,
                   szFilter,
                   this);
	if(dlg.DoModal() == IDOK) {

		char buf[255], t_buf[1000], f_buf[255*100];
		CString ti_buf;

		int nh = wmtrnet->GetMax();
	
		strcpy(f_buf,  "|--------------------------------------------------------------------|\r\n");
		sprintf(t_buf, "|                            WinMTR statistics                       |\r\n");
		strcat(f_buf, t_buf);
		sprintf(t_buf, "|         IP      -   Loss%%| Sent | Recv | Best | Avrg | Worst| Last |\r\n" ); 
		strcat(f_buf, t_buf);
		sprintf(t_buf, "|--------------------------|------|------|------|------|------|------|\r\n" ); 
		strcat(f_buf, t_buf);

		for(int i=0;i <nh ; i++) {
			wmtrnet->GetIP(i, buf);
		
			sprintf(t_buf, "|%16s -   %4d | %4d | %4d | %4d | %4d | %4d | %4d |\r\n" , 
					buf, wmtrnet->GetPercent(i),
					wmtrnet->GetXmit(i), wmtrnet->GetReturned(i), wmtrnet->GetBest(i),
					wmtrnet->GetAvg(i), wmtrnet->GetWorst(i), wmtrnet->GetLast(i));
			strcat(f_buf, t_buf);
		}	
	
		sprintf(t_buf, "|__________________________|______|______|______|______|______|______|\r\n" ); 
		strcat(f_buf, t_buf);

	
		CString cs_tmp((LPCSTR)IDS_STRING_SB_NAME);
		strcat(f_buf, "                   ");
		strcat(f_buf, (LPCTSTR)cs_tmp);

		CTime tm = CTime::GetCurrentTime();
		ti_buf = tm.Format("  %d/%m/%Y %X\r\n");
		strcat(f_buf, ti_buf);

		FILE *fp = fopen(dlg.GetPathName(), "wt");
		if(fp != NULL) {
			fprintf(fp, "%s", f_buf);
			fclose(fp);
		}
	}
}


//*****************************************************************************
// WinMTRDialog::OnEXPH
//
// 
//*****************************************************************************
void WinMTRDialog::OnEXPH() 
{
   TCHAR BASED_CODE szFilter[] = _T("HTML Files (*.htm, *.html)|*.htm;*.html|All Files (*.*)|*.*||");

   CFileDialog dlg(FALSE,
                   _T("HTML"),
                   NULL,
                   OFN_HIDEREADONLY | OFN_EXPLORER,
                   szFilter,
                   this);

	if(dlg.DoModal() == IDOK) {

		char buf[255], t_buf[1000], f_buf[255*100];
		CString ti;

		int nh = wmtrnet->GetMax();
	
		strcpy(f_buf, "<html><head><title>WinMTR Statistics</title></head><body bgcolor=\"white\">\r\n");
		sprintf(t_buf, "<center><h2>WinMTR statistics</h2></center>\r\n");
		CTime tm = CTime::GetCurrentTime();
		ti = tm.Format("<center><h3>%d/%m/%Y %X</h3></center>\r\n");
		strcat(t_buf, ti);

		strcat(f_buf, t_buf);
	
		sprintf(t_buf, "<p align=\"center\"> <table border=\"1\" align=\"center\">\r\n" ); 
		strcat(f_buf, t_buf);
	
		sprintf(t_buf, "<tr><td>#</td> <td>IP</td> <td>Loss%%</td> <td>Sent</td> <td>Recv</td> <td>Best</td> <td>Avrg</td> <td>Worst</td> <td>Last</td></tr>\r\n" ); 
		strcat(f_buf, t_buf);

		for(int i=0;i <nh ; i++) {
			wmtrnet->GetIP(i, buf);
		
			sprintf(t_buf, "<tr><td>%4d</td> <td>%s</td> <td>%4d</td> <td>%4d</td> <td>%4d</td> <td>%4d</td> <td>%4d</td> <td>%4d</td> <td>%4d</td></tr>\r\n" , 
					i,buf, wmtrnet->GetPercent(i),
					wmtrnet->GetXmit(i), wmtrnet->GetReturned(i), wmtrnet->GetBest(i),
					wmtrnet->GetAvg(i), wmtrnet->GetWorst(i), wmtrnet->GetLast(i));
			strcat(f_buf, t_buf);
		}	


		sprintf(t_buf, "</table></body></html>\r\n"); 
		strcat(f_buf, t_buf);

		FILE *fp = fopen(dlg.GetPathName(), "wt");
		if(fp != NULL) {
			fprintf(fp, "%s", f_buf);
			fclose(fp);
		}
	}


}


//*****************************************************************************
// WinMTRDialog::WinMTRDialog
//
// 
//*****************************************************************************
void WinMTRDialog::OnCancel() 
{
}


//*****************************************************************************
// WinMTRDialog::DisplayRedraw
//
// 
//*****************************************************************************
int WinMTRDialog::DisplayRedraw()
{
	char buf[255], nbuf[255], nr_crt[255];
	memset(buf,0,sizeof(buf));
	int nh = wmtrnet->GetMax();
	while( m_listMTR.GetItemCount() > nh ) m_listMTR.DeleteItem(m_listMTR.GetItemCount() - 1);

	for (int i = 0; i < nh; i++) {

		sprintf(nr_crt, "%d", i + 1);

		if (m_listMTR.GetItemCount() <= i){
		m_listMTR.InsertItem(i, buf);
	}
		else{
			wmtrnet->GetIP(i, nbuf);
			wmtrnet->GetName(i, buf);
			if (nr_crt != m_listMTR.GetItemText(i, 0))
			{
				m_listMTR.SetItem(i, 0, LVIF_TEXT, nr_crt, 0, 0, 0, 0);
			}
			if (nbuf != m_listMTR.GetItemText(i, 1))
			{
				m_listMTR.SetItem(i, 1, LVIF_TEXT, nbuf, 0, 0, 0, 0);
			}
			if (buf != m_listMTR.GetItemText(i, 2))
			{
				m_listMTR.SetItem(i, 2, LVIF_TEXT, buf, 0, 0, 0, 0);
			}

			sprintf(buf, "%d", wmtrnet->GetPercent(i));
			m_listMTR.SetItem(i, 3, LVIF_TEXT, buf, 0, 0, 0, 0);

			sprintf(buf, "%d", wmtrnet->GetXmit(i));
			m_listMTR.SetItem(i, 4, LVIF_TEXT, buf, 0, 0, 0, 0);

			sprintf(buf, "%d", wmtrnet->GetReturned(i));
			m_listMTR.SetItem(i, 5, LVIF_TEXT, buf, 0, 0, 0, 0);

			sprintf(buf, "%d", wmtrnet->GetBest(i));
			m_listMTR.SetItem(i, 6, LVIF_TEXT, buf, 0, 0, 0, 0);

			sprintf(buf, "%d", wmtrnet->GetAvg(i));
			m_listMTR.SetItem(i, 7, LVIF_TEXT, buf, 0, 0, 0, 0);

			sprintf(buf, "%d", wmtrnet->GetWorst(i));
			m_listMTR.SetItem(i, 8, LVIF_TEXT, buf, 0, 0, 0, 0);

			sprintf(buf, "%d", wmtrnet->GetLast(i));
			m_listMTR.SetItem(i, 9, LVIF_TEXT, buf, 0, 0, 0, 0);

		}
	}

	return 0;
}


//*****************************************************************************
// WinMTRDialog::InitMTRNet
//
// 
//*****************************************************************************
int WinMTRDialog::InitMTRNet()
{
	char strtmp[255];
	char *Hostname = strtmp;
	char buf[255];
	struct hostent *host;
	m_comboHost.GetWindowText(strtmp, 255);
   	
	if (Hostname == NULL) Hostname = "localhost";
   
	int isIP=1;
	char *t = Hostname;
	while(*t) {
		if(!isdigit(*t) && *t!='.') {
			isIP=0;
			break;
		}
		t++;
	}

	if(!isIP) {
		sprintf(buf, "Resolving host %s...", strtmp);
		statusBar.SetPaneText(0,buf);
		host = gethostbyname(Hostname);
		if(host == NULL) {
			statusBar.SetPaneText(0, CString((LPCSTR)IDS_STRING_SB_NAME) );
			AfxMessageBox("Unable to resolve hostname.");
			return 0;
		}
	}

	return 1;
}


//*****************************************************************************
// PingThread
//
// 
//*****************************************************************************
void PingThread(void *p)
{
	WinMTRDialog *wmtrdlg = (WinMTRDialog *)p;
	WaitForSingleObject(wmtrdlg->traceThreadMutex, INFINITE);

	struct hostent *host, *lhost;
	char strtmp[255];
	char *Hostname = strtmp;
	int traddr;
	int localaddr;

	wmtrdlg->m_comboHost.GetWindowText(strtmp, 255);
   	
	if (Hostname == NULL) Hostname = "localhost";
   
	int isIP=1;
	char *t = Hostname;
	while(*t) {
		if(!isdigit(*t) && *t!='.') {
			isIP=0;
			break;
		}
		t++;
	}

	if(!isIP) {
      host = gethostbyname(Hostname);
      traddr = *(int *)host->h_addr;
	} else
      traddr = inet_addr(Hostname);

	lhost = gethostbyname("localhost");
	if(lhost == NULL) {
      AfxMessageBox("Unable to get local IP address.");
      ReleaseMutex(wmtrdlg->traceThreadMutex);
      return;
	}
	localaddr = *(int *)lhost->h_addr;
	
	wmtrdlg->wmtrnet->DoTrace(traddr);

	ReleaseMutex(wmtrdlg->traceThreadMutex);
   _endthread();
}



void WinMTRDialog::OnCbnSelchangeComboHost()
{
}

void WinMTRDialog::ClearHistory()
{
	HKEY hKey;
	DWORD tmp_dword;
	LONG r;
	char key_name[20];

	r = RegOpenKeyEx(	HKEY_CURRENT_USER, "Software", 0, KEY_ALL_ACCESS,&hKey);
	r = RegOpenKeyEx(	hKey, "WinMTR", 0, KEY_ALL_ACCESS, &hKey);
	r = RegOpenKeyEx(	hKey, "LRU", 0, KEY_ALL_ACCESS, &hKey);

	for(int i = 0; i<=nrLRU; i++) {
		sprintf(key_name, "Host%d", i);
		r = RegDeleteValue(hKey,key_name);
	}
	nrLRU = 0;
	tmp_dword = nrLRU;
	r = RegSetValueEx(hKey,"NrLRU", 0, REG_DWORD, (const unsigned char *)&tmp_dword, sizeof(DWORD));
	RegCloseKey(hKey);

	m_comboHost.Clear();
	m_comboHost.ResetContent();
	m_comboHost.AddString(CString((LPCSTR)IDS_STRING_CLEAR_HISTORY));
}

void WinMTRDialog::OnCbnSelendokComboHost()
{
}


void WinMTRDialog::OnCbnCloseupComboHost()
{
	if(m_comboHost.GetCurSel() == m_comboHost.GetCount() - 1) {
		ClearHistory();
	}
}

void WinMTRDialog::Transit(STATES new_state)
{
	switch(new_state) {
		case IDLE:
			switch (state) {
				case STOPPING:
					transition = STOPPING_TO_IDLE;
				break;
				case IDLE:
					transition = IDLE_TO_IDLE;
				break;
				default:
					TRACE_MSG("Received state IDLE after " << state);
					return;
			}
			state = IDLE;
		break;
		case TRACING:
			switch (state) {
				case IDLE:
					transition = IDLE_TO_TRACING;
				break;
				case TRACING:
					transition = TRACING_TO_TRACING;
				break;
				default:
					TRACE_MSG("Received state TRACING after " << state);
					return;
			}
			state = TRACING;
		break;
		case STOPPING:
			switch (state) {
				case STOPPING:
					transition = STOPPING_TO_STOPPING;
				break;
				case TRACING:
					transition = TRACING_TO_STOPPING;
				break;
				default:
					TRACE_MSG("Received state STOPPING after " << state);
					return;
			}
			state = STOPPING;
		break;
		case EXIT:
			switch (state) {
				case IDLE:
					transition = IDLE_TO_EXIT;
				break;
				case STOPPING:
					transition = STOPPING_TO_EXIT;
				break;
				case TRACING:
					transition = TRACING_TO_EXIT;
				break;
				case EXIT:
				break;
				default:
					TRACE_MSG("Received state EXIT after " << state);
					return;
			}
			state = EXIT;
		break;
		default:
			TRACE_MSG("Received state " << state);
	}

	// modify controls according to new state
	switch(transition) {
		case IDLE_TO_TRACING:
			m_buttonStart.EnableWindow(FALSE);
			m_buttonStart.SetWindowText("Stop");
			m_comboHost.EnableWindow(FALSE);
			m_buttonOptions.EnableWindow(FALSE);
			statusBar.SetPaneText(0, "Double click or right click for more information.");
			_beginthread(PingThread, 0 , this);
			m_buttonStart.EnableWindow(TRUE);
		break;
		case IDLE_TO_IDLE:
			// nothing to be done
		break;
		case STOPPING_TO_IDLE:
			m_buttonStart.EnableWindow(TRUE);
			statusBar.SetPaneText(0, CString((LPCSTR)IDS_STRING_SB_NAME) );
			m_buttonStart.SetWindowText("Start");
			m_comboHost.EnableWindow(TRUE);
			m_buttonOptions.EnableWindow(TRUE);
			m_comboHost.SetFocus();
		break;
		case STOPPING_TO_STOPPING:
			DisplayRedraw();
		break;
		case TRACING_TO_TRACING:
			DisplayRedraw();
		break;
		case TRACING_TO_STOPPING:
			m_buttonStart.EnableWindow(FALSE);
			m_comboHost.EnableWindow(FALSE);
			m_buttonOptions.EnableWindow(FALSE);
			wmtrnet->StopTrace();
			statusBar.SetPaneText(0, "Waiting for last packets in order to stop trace ...");
			DisplayRedraw();
		break;
		case IDLE_TO_EXIT:
			m_buttonStart.EnableWindow(FALSE);
			m_comboHost.EnableWindow(FALSE);
			m_buttonOptions.EnableWindow(FALSE);
		break;
		case TRACING_TO_EXIT:
			m_buttonStart.EnableWindow(FALSE);
			m_comboHost.EnableWindow(FALSE);
			m_buttonOptions.EnableWindow(FALSE);
			wmtrnet->StopTrace();
			statusBar.SetPaneText(0, "Waiting for last packets in order to stop trace ...");
		break;
		case STOPPING_TO_EXIT:
			m_buttonStart.EnableWindow(FALSE);
			m_comboHost.EnableWindow(FALSE);
			m_buttonOptions.EnableWindow(FALSE);
		break;
		default:
			TRACE_MSG("Unknown transition " << transition);
	}
}


void WinMTRDialog::OnTimer(UINT_PTR nIDEvent)
{
	static unsigned int call_count = 0;

	if(state == EXIT && WaitForSingleObject(traceThreadMutex, 0) == WAIT_OBJECT_0) {
		ReleaseMutex(traceThreadMutex);
		OnOK();
	}


	if( WaitForSingleObject(traceThreadMutex, 0) == WAIT_OBJECT_0 ) {
		ReleaseMutex(traceThreadMutex);
		Transit(IDLE);
    } else if ((call_count++ % 10 == 0) && (WaitForSingleObject(traceThreadMutex, 0) == WAIT_TIMEOUT)) {
		ReleaseMutex(traceThreadMutex);
        if (state == TRACING) {
            Transit(TRACING);
        }
        else if (state == STOPPING) {
            Transit(STOPPING);
        }
    }

	CDialog::OnTimer(nIDEvent);
}


void WinMTRDialog::OnClose()
{
	Transit(EXIT);
}


void WinMTRDialog::OnBnClickedCancel()
{
	Transit(EXIT);
}




void WinMTRDialog::OnRclickListMtr(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMITEMACTIVATE pNMItemActivate = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);
	// TODO:  在此添加控件通知处理程序代码
	*pResult = 0;

	NM_LISTVIEW* pNMListView = (NM_LISTVIEW*)pNMHDR;
	if (pNMListView->iItem != -1)
	{
		DWORD dwPos = GetMessagePos();
		CPoint point(LOWORD(dwPos), HIWORD(dwPos));
		CMenu menu;
		//添加线程操作
		VERIFY(menu.LoadMenu(IDR_RCMENU));			//这里是我们在1中定义的MENU的文件名称
		CMenu* popup = menu.GetSubMenu(0);
		ASSERT(popup != NULL);
		popup->TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, point.x, point.y, this);


	}
}



void WinMTRDialog::OnMenuCopyip()
{
	// TODO:  在此添加命令处理程序代码
	// TODO:  在此添加命令处理程序代码
	POSITION m_pstion = m_listMTR.GetFirstSelectedItemPosition();
	int m_nIndex = m_listMTR.GetNextSelectedItem(m_pstion);
	
	if (wmtrnet->GetAddr(m_nIndex)!=0)
	{
		char buf[255];
		wmtrnet->GetIP(m_nIndex, buf);
		HGLOBAL clipbuffer;
		char * buffer;
		CString source(buf);

		OpenClipboard();
		EmptyClipboard();

		clipbuffer = GlobalAlloc(GMEM_DDESHARE, source.GetLength() + 1);
		buffer = (char*)GlobalLock(clipbuffer);
		strcpy(buffer, LPCSTR(source));
		GlobalUnlock(clipbuffer);

		SetClipboardData(CF_TEXT, clipbuffer);
		CloseClipboard();
	}
}


void WinMTRDialog::OnMenuIpip()
{
	// TODO:  在此添加命令处理程序代码
	POSITION m_pstion = m_listMTR.GetFirstSelectedItemPosition();
	int m_nIndex = m_listMTR.GetNextSelectedItem(m_pstion);
	if (wmtrnet->GetAddr(m_nIndex) != 0)
	{
		char buf[255], ipnet[255];
		wmtrnet->GetIP(m_nIndex, buf);

		strcpy(ipnet, "https://www.ipip.net/ip/");
		strcat(ipnet, buf);
		strcat(ipnet, ".html");

		ShellExecute(NULL, "open", ipnet, NULL, NULL, SW_SHOW);

	}
}


void WinMTRDialog::OnMenuWhois()
{
	// TODO:  在此添加命令处理程序代码
	POSITION m_pstion = m_listMTR.GetFirstSelectedItemPosition();
	int m_nIndex = m_listMTR.GetNextSelectedItem(m_pstion);
	if (wmtrnet->GetAddr(m_nIndex) != 0)
	{
		char buf[255], he[255];
		wmtrnet->GetIP(m_nIndex, buf);

		strcpy(he, "https://bgp.he.net/ip/");
		strcat(he, buf);

		ShellExecute(NULL, "open", he, NULL, NULL, SW_SHOW);

	}
}

BOOL WinMTRDialog::OnHelpInfo(HELPINFO* pHelpInfo)
{
	// TODO:  在此添加消息处理程序代码和/或调用默认值
	WinMTRHelp helpDlg;
	helpDlg.DoModal();
	return true;
}
