//*****************************************************************************
// FILE:            WinMTROptions.h
//
//
// DESCRIPTION:
//   
//
// NOTES:
//    
//
//*****************************************************************************

#ifndef WINMTROPTIONS_H_
#define WINMTROPTIONS_H_



//*****************************************************************************
// CLASS:  WinMTROptions
//
//
//*****************************************************************************

class WinMTROptions : public CDialog
{
public:
	WinMTROptions(CWnd* pParent = NULL);

	void SetUseDNS(BOOL udns)		{ useDNS = udns;  };
	void SetInterval(double i)		{ interval = i;   };
	void SetPingSize(int ps)		{ pingsize = ps;  };
	void SetCountsize(int c)		{ countsize = c; };
	void SetMaxLRU(int mlru)		{ maxLRU = mlru;  };

	double GetInterval()			{ return interval;   };
	int GetPingSize()				{ return pingsize;   };
	int GetMaxLRU()					{ return maxLRU;   };
	int GetCountsize()				{ return countsize; };
	BOOL GetUseDNS()				{ return useDNS;     };

	enum { IDD = IDD_DIALOG_OPTIONS };
	CEdit	m_editSize;
	CEdit	m_editInterval;
	CEdit	m_editMaxLRU;
	CEdit	m_editCountsize;
	CButton	m_checkDNS;

protected:
	virtual void DoDataExchange(CDataExchange* pDX);

	virtual BOOL OnInitDialog();
	virtual void OnOK();

	afx_msg void OnLicense();

	DECLARE_MESSAGE_MAP()
	
private:
	double   interval;
	int      pingsize;
	int		 maxLRU;
	int		 countsize;
	BOOL     useDNS;
public:
	afx_msg BOOL OnHelpInfo(HELPINFO* pHelpInfo);
};

#endif // ifndef WINMTROPTIONS_H_
