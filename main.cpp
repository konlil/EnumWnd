#include "main.h"
#include <string>
#include "psapi.h"
#include <Tlhelp32.h>
#include <vector>
#pragma comment(linker, "/defaultlib:Psapi.lib")
#pragma comment(linker, "/defaultlib:version.lib")

//----------------
//ȫ�ֱ�������
//-----------------
HINSTANCE g_hInst	=	NULL;
//���Ի���Ĵ��ھ��
HWND g_hDlg			=	NULL;
//���οؼ��ľ��
HWND g_hTree		=	NULL;
//�Ի���ı�֮ǰ�Ĵ�С
SIZE g_siDlg		=	{0};
//�Ի���ԭʼ��С
SIZE g_siOrig		=	{0};
BOOL g_bInited		=	FALSE;
//�����϶��Ŀؼ�����
RECT g_rcDragIcon	=	{0};
//�϶�ʱ�Ĺ��
HICON g_hDragIcon	=	NULL;
//����Ƿ����϶�״̬
BOOL g_bDragging		=	FALSE;
//�������
HICON g_hNormalIcon =	NULL;
//����ͼ��
HICON g_hWndIcon	=	NULL;

BOOL g_showLayered  = FALSE;
BOOL g_showVisible = FALSE;

typedef HANDLE (WINAPI * PFNCREATETOOLHELP32SNAPSHOT)( DWORD dwFlags, DWORD th32ProcessID );
typedef BOOL (WINAPI * PFNPROCESS32FIRST)( HANDLE hSnapshot, LPPROCESSENTRY32 lppe );
typedef BOOL (WINAPI * PFNPROCESS32NEXT)( HANDLE hSnapshot, LPPROCESSENTRY32 lppe );
typedef BOOL (WINAPI * PFNMODULE32FIRST)( HANDLE hSnapshot, LPMODULEENTRY32 lpme );
typedef BOOL (WINAPI * PFNMODULE32NEXT)( HANDLE hSnapshot, LPMODULEENTRY32 lpme );
typedef DWORD (WINAPI *PFNGETPROCESSIMAGEFILENAME)( HANDLE hProcess, LPTSTR lpImageFileName, DWORD nSize );
typedef DWORD (WINAPI *PFNGETMODULEFILENAMEEX)(HANDLE hProcess,HMODULE hModule,LPTSTR lpFilename,DWORD nSize);

struct ModuleInfo
{
	std::string name;
	HMODULE 	hmodule;
	DWORD		base_address;
	DWORD		length;
	std::string desc;
	std::string path;
};

struct ProcessInfo
{
	DWORD	pid;
	std::string name;
	std::vector<ModuleInfo> modules;
};

struct TRANSLATION {
	WORD langID;
	WORD charset;
};

//------------------
//����ʵ��
//------------------
bool get_info( void * buf , const char * value , char ** result_info , UINT * len )
{
	BOOL result = FALSE;
	TRANSLATION * lp_lan = NULL;
	result = ::VerQueryValue(buf,"\\VarFileInfo\\Translation",(LPVOID*)&lp_lan,len);
	if( !result )
		return false;

	char tmp_name[128];
	sprintf( tmp_name, "\\StringFileInfo\\%04x%04x\\%s", lp_lan->langID, lp_lan->charset , value );
	result = ::VerQueryValue( buf, tmp_name, (LPVOID*)result_info , len );
	return result == FALSE ? false : true;
}

// get_process_info
void get_process_info( int pid , ProcessInfo& pi )
{
	PFNCREATETOOLHELP32SNAPSHOT pfnCreateToolhelp32Snapshot_;
	PFNPROCESS32FIRST pfnProcess32First_;
	PFNPROCESS32NEXT  pfnProcess32Next_;
	PFNMODULE32FIRST  pfnModule32First_;
	PFNMODULE32NEXT   pfnModule32Next_;
	PFNGETPROCESSIMAGEFILENAME pfnGetProcessImageFileName_;
	PFNGETMODULEFILENAMEEX pfnGetModuleFileNameEx_;

	HMODULE module = ::LoadLibrary( "kernel32.dll" );
	pfnCreateToolhelp32Snapshot_ = (PFNCREATETOOLHELP32SNAPSHOT) GetProcAddress(module, "CreateToolhelp32Snapshot");
	pfnProcess32First_ = (PFNPROCESS32FIRST)GetProcAddress(module, "Process32First");
	pfnProcess32Next_ = (PFNPROCESS32NEXT)GetProcAddress(module, "Process32Next");
	pfnModule32First_ = (PFNMODULE32FIRST)GetProcAddress(module, "Module32First");
	pfnModule32Next_ = (PFNMODULE32NEXT)GetProcAddress(module, "Module32Next");
	
	module = ::LoadLibrary( "psapi.dll" );
	pfnGetProcessImageFileName_ = (PFNGETPROCESSIMAGEFILENAME)GetProcAddress(module, "GetProcessImageFileNameA");
	pfnGetModuleFileNameEx_ = (PFNGETMODULEFILENAMEEX)GetProcAddress(module, "GetModuleFileNameExA");
	
	HANDLE hSnapShot = pfnCreateToolhelp32Snapshot_(TH32CS_SNAPPROCESS,NULL);
	PROCESSENTRY32 pe32 = { sizeof(PROCESSENTRY32) };
	ModuleInfo mi;

	for( BOOL run = pfnProcess32First_(hSnapShot,&pe32) ; run ; run = pfnProcess32Next_(hSnapShot,&pe32) )
	{
		if( pe32.th32ProcessID == pid )
		{
			pi.name = pe32.szExeFile;
			pi.pid = pe32.th32ProcessID;
			//sprintf(szText, "process path: %s\n", pe32.szExeFile);
			//OutputDebugString(szText);
			HANDLE hProcessSnapShot = pfnCreateToolhelp32Snapshot_(TH32CS_SNAPMODULE,pe32.th32ProcessID);
			MODULEENTRY32 me = { sizeof(MODULEENTRY32) };
			pfnModule32First_(hProcessSnapShot,&me);
			mi.name = me.szModule;
			mi.base_address = (DWORD)me.modBaseAddr;
			mi.length = me.modBaseSize;
			mi.hmodule = me.hModule;
			mi.path = me.szExePath;
			char * info = NULL;
			DWORD info_size = ::GetFileVersionInfoSize(LPCSTR(me.szExePath),NULL);
			char *buf = (char*)alloca(info_size);
			if( ::GetFileVersionInfo(LPCSTR(me.szExePath),NULL,info_size,buf) )
			{
				UINT len = 0;
				if(get_info((void *)buf, "CompanyName", &info, &len))
				{
					mi.desc = info;
				}
				else
				{
					mi.desc="";
				}
			}
			pi.modules.push_back(mi);
				//sprintf(szText, "moudle path: %s\n", me.szExePath);
				//OutputDebugString(szText);
			if( hProcessSnapShot!= INVALID_HANDLE_VALUE )
				::CloseHandle( hProcessSnapShot );
			break;
		}
	}
	if( hSnapShot != INVALID_HANDLE_VALUE )
	{
		::CloseHandle(hSnapShot);
	}
}

//Ӧ�ó������ں���
int APIENTRY WinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPSTR     lpCmdLine,
                     int       nCmdShow)
{
	g_hInst = hInstance;
	//��ʼ�������ؼ�
	InitCommonControls();
	//����һ��ģʽ�Ի��򣬲��ȴ��Ի������
	DialogBox(hInstance, MAKEINTRESOURCE(IDD_MAIN), NULL, DlgProc);
	return 0;
}
//�Ի�����̺���
INT_PTR CALLBACK 
DlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg) 
	{
	case WM_INITDIALOG:		//�Ի����ʼ��
		//���洰�ھ��
		g_hDlg = hwndDlg;		
		OnInitDialog();
		return 1;
	case WM_COMMAND:		//��������
		OnCommand(wParam, lParam);
		return 1;
	case WM_CLOSE:			//�رնԻ���
		EndDialog(hwndDlg, 0);
		return 1;
	case WM_SIZE:			//�Ի����С�ı�
		OnSize(wParam, lParam);
		return 1;
		break;
	case WM_GETMINMAXINFO:	//��ѯ���������С�ߴ�
		{
			//���ƴ�����С�ߴ�
			LPMINMAXINFO lpmm = (LPMINMAXINFO)lParam;
			lpmm->ptMinTrackSize.x = g_siOrig.cx;
			lpmm->ptMinTrackSize.y = g_siOrig.cy;
		}
		return 1;
	case WM_LBUTTONDOWN:	//����������
		OnLButtonDown(wParam, LOWORD(lParam), HIWORD(lParam));
		return 1;
	case WM_LBUTTONUP:		//����Ҽ�����
		OnLButtonUp(wParam, LOWORD(lParam), HIWORD(lParam));
		return 1;
	case WM_ACTIVATE:		//���ڻ״̬�ı�
		if(wParam == WA_INACTIVE && g_bDragging)
		{
			//����϶�;�д���ʧȥ���㣬���������϶�
			BOOL b = ReleaseCapture();
			g_bDragging = FALSE;
		}
		return 1;
	case WM_NOTIFY:			//�ؼ�֪ͨ��Ϣ
		if(wParam == IDC_TREE)
		{
			//ֻ������״�ؼ���ѡ�иı�֪ͨ
			LPNMHDR pnmh = (LPNMHDR)lParam;
			if(pnmh->code == TVN_SELCHANGED)
			{
				OnTreeSelChanged(pnmh);
				return 1;
			}
		}
		break;
	default:
		break;
	}
	//Ĭ�Ϸ���0
	return 0;
}
//�жϴ����Ƿ�ɼ�
BOOL WindowCanBeSee(HWND hwnd)
{
	if(hwnd == GetDesktopWindow())
		return TRUE;

	if( !IsWindowVisible(hwnd) )
		return FALSE;

	bool got = false;
	//��ȡ�����ı�
	char szText[1024] = {0};
	GetWindowText(hwnd, szText, MAX_PATH);	
	if( strcmp(szText, "DSDock") == 0 )
	{
		got = true;	
	}

	RECT sRect = {0};
	GetWindowRect(hwnd, &sRect);
	if( got ){
		//sprintf(szText, "srect: %ld, %ld, %ld, %ld\n", sRect.left, sRect.right, sRect.top, sRect.bottom);
		//OutputDebugString(szText);
	}

	RECT tRect;
	RECT r;
	HWND h = GetTopWindow(0);
	GetWindowRect(h, &r);
	tRect = r;
	while( h && hwnd != h )
	{
		h = GetNextWindow(h, GW_HWNDNEXT);
		GetWindowRect(h, &r);
		tRect.left = tRect.left > r.left ? r.left : tRect.left;
		tRect.right = tRect.right < r.right ? r.right : tRect.right;
		tRect.bottom = tRect.bottom < r.bottom ? r.bottom : tRect.bottom;
		tRect.top = tRect.top > r.top ? r.top : tRect.top;

		if( got )
		{
			//sprintf(szText, "update trect: %ld, %ld, %ld, %ld\n", tRect.left, tRect.right, tRect.top, tRect.bottom);
			//OutputDebugString(szText);
		}
	}

	if ( hwnd == h ) {
		if( sRect.left >= tRect.left &&
			sRect.right <= tRect.right &&
			sRect.top >= tRect.top &&
			sRect.bottom <= tRect.bottom )
			return FALSE;
	}
	return TRUE;
}
//��ʼ���Ի���
void OnInitDialog()
{
	//���洰��ԭʼ�ߴ�
	RECT rc = {0};
	GetWindowRect(g_hDlg, &rc);
	g_siOrig.cx = rc.right - rc.left;
	g_siOrig.cy = rc.bottom - rc.top;
	//���洰�ڿͻ�����С
	GetClientRect(g_hDlg, &rc);
	g_siDlg.cx = rc.right - rc.left;
	g_siDlg.cy = rc.bottom - rc.top;
	//������״�ؼ����
	g_hTree = GetDlgItem(g_hDlg, IDC_TREE);
	//�����϶����
	g_hDragIcon		= LoadIcon(g_hInst, MAKEINTRESOURCE(IDI_DRAG));
	//�����������
	g_hNormalIcon	= LoadCursor(NULL, IDC_ARROW);
	//���ش���ͼ��
	g_hWndIcon		= LoadIcon(g_hInst, MAKEINTRESOURCE(IDI_MAIN));
	//���ô��ڴ�ͼ��
	SendMessage(g_hDlg, WM_SETICON, ICON_BIG, (LPARAM)g_hWndIcon);
	//���ô���Сͼ��
	SendMessage(g_hDlg, WM_SETICON, ICON_SMALL, (LPARAM)g_hWndIcon);
	//��ȡ�϶�ͼ���Ӧ��Χ
	HWND hCtrl = GetDlgItem(g_hDlg, IDC_DRAG);
	GetWindowRect(hCtrl, &g_rcDragIcon);
	ScreenToClientRect(&g_rcDragIcon);
	g_bInited = TRUE;
}
//�Ի���������Ӧ
void OnCommand(WPARAM wp, LPARAM lp)
{
	int nID = LOWORD(wp);
	switch(nID)
	{
	//������ˢ�¡���ť�����б�
	case IDOK:
		GetWindowList(NULL);
		break;
	//�������رա���ťʱ�����Ի���
	case IDCANCEL:
		SendMessage(g_hDlg, WM_CLOSE, 0, 0);
		break;
	case IDC_CHECK_VISIBLE:
		g_showVisible = !g_showVisible;
		break;
	case IDC_CHECK_LAYERED:
		g_showLayered = !g_showLayered;
		break;
	}
}
//WM_SIZE��Ϣ��Ӧ
void OnSize(WPARAM wp, LPARAM lp)
{
	//ֻ�����Ҫ����Ϣ�����ڷǱ�Ҫ��Ϣֱ�ӷ���
	switch(wp)
	{
	case SIZE_MAXHIDE:
	case SIZE_MAXSHOW:
	case SIZE_MINIMIZED:
		return;
	}
	//ȡ�öԻ�����´�С
	int nWidth = LOWORD(lp);
	int nHeight = HIWORD(lp);
	//��øı�Ĳ�ֵ
	int x, y;
	x = nWidth - g_siDlg.cx;
	y = nHeight - g_siDlg.cy;
	//��ʼ�ƶ��ؼ�������Ӧ�ĳ�ʼ��
	HDWP hDwp = BeginDeferWindowPos(5);
	if(hDwp == NULL)
		return;
	//�ƶ����οؼ�
	hDwp = MoveCtrl(hDwp, IDC_TREE, x, y, FALSE, FALSE, FALSE, TRUE);
	//�ƶ��ı���
	hDwp = MoveCtrl(hDwp, IDC_EDT_DETAIL, x, y, FALSE, FALSE, TRUE, TRUE);
	//�ƶ���ö�����д��塱��ť
	hDwp = MoveCtrl(hDwp, IDOK, x, y, FALSE, TRUE, FALSE, TRUE);
	//�ƶ����϶���ͼ��
	hDwp = MoveCtrl(hDwp, IDC_DRAG, x, y, FALSE, TRUE, FALSE, TRUE);
	//�ƶ���ʾ�ı�
	hDwp = MoveCtrl(hDwp, IDC_TIP, x, y, FALSE, TRUE, FALSE, TRUE);
	hDwp = MoveCtrl(hDwp, IDC_CHECK_VISIBLE, x, y, FALSE, TRUE, FALSE, TRUE);
	hDwp = MoveCtrl(hDwp, IDC_CHECK_LAYERED, x, y, FALSE, TRUE, FALSE, TRUE);
	//�����ƶ���ʹ���пؼ�һ��������ƶ�
	EndDeferWindowPos(hDwp);
	//�����϶�ͼ���Ӧ��Χ
	HWND hCtrl = GetDlgItem(g_hDlg, IDC_DRAG);
	GetWindowRect(hCtrl, &g_rcDragIcon);
	ScreenToClientRect(&g_rcDragIcon);
	//���浱ǰ��С
	g_siDlg.cx = nWidth;
	g_siDlg.cy = nHeight;
}
//����ָ�������ƶ�ĳ���ؼ�
HDWP MoveCtrl(HDWP hDwp, UINT nID, int x, int y, 
		BOOL bLeft, BOOL bTop, BOOL bRight, BOOL bBottom)
{
	//��ȡ�ؼ����
	HWND hCtrl = GetDlgItem(g_hDlg, nID);
	//ȡ�ÿؼ�λ�úʹ�С
	RECT rc = {0};
	GetWindowRect(hCtrl, &rc);
	//����Ļ����ת��Ϊ��������
	ScreenToClientRect(&rc);
	//����ѡ��ı�λ�úʹ�С
	if(bLeft)
		rc.left += x;
	if(bTop)
		rc.top += y;
	if(bRight)
		rc.right += x;
	if(bBottom)
		rc.bottom += y;
	//�ƶ��ؼ�
	return DeferWindowPos(hDwp, hCtrl, 0, rc.left, rc.top, 
		rc.right - rc.left, rc.bottom - rc.top, SWP_NOZORDER);
}
//WM_LBUTTONDOWN��Ϣ��Ӧ
void OnLButtonDown(UINT nType, int x, int y)
{
	//��������϶��ؼ���Ӧ��Χ�ڰ���ʱ�������϶�״̬
	if(x >= g_rcDragIcon.left && x <= g_rcDragIcon.right 
		&& y >= g_rcDragIcon.top && y <= g_rcDragIcon.bottom)
	{
		g_bDragging = TRUE;
		//�ı���
		SetCursor(g_hDragIcon);
		//��ʼ��׽���
		SetCapture(g_hDlg);
	}
}
//WM_LBUTTONUP��Ϣ��Ӧ
void OnLButtonUp(UINT nType, int x, int y)
{
	if(g_bDragging)
	{
		//�ָ����
		SetCursor(g_hNormalIcon);
		//�ͷ���겶��
		ReleaseCapture();
		//�õ���ǰ���λ��
		POINT pt = {x, y};
		//����������ת��Ϊ��Ļ����
		ClientToScreen(g_hDlg, &pt);
		//��ȡ������Ĵ��ھ��
		HWND hWnd = WindowFromPoint(pt);
		if(hWnd != NULL)
			GetWindowList(hWnd);
		else
			MessageBeep(MB_ICONEXCLAMATION);
	}
}
//���οؼ�ѡ����ı���Ϣ
void OnTreeSelChanged(LPNMHDR pnmh)
{
	//��ȡ��Ϣ����
	LPNMTREEVIEW pnmtv = (LPNMTREEVIEW)pnmh;
	//��ȡ��Ϊѡ��״̬�Ľڵ��б���Ĵ��ھ��
	HWND hWnd = (HWND)pnmtv->itemNew.lParam;
	//��ȡ������Ϣ
	TCHAR	szBuf[1024]			= {0};
	RECT	rc					= {0};
	TCHAR	szClass[MAX_PATH]	= {0};
	TCHAR	szText[MAX_PATH]	= {0};
	//��ȡ����λ��
	GetWindowRect(hWnd, &rc);
	//��ȡ��������
	GetClassName(hWnd, szClass, MAX_PATH);
	//��ȡ�����ı�
	GetWindowText(hWnd, szText, MAX_PATH);
	//
	long el = GetWindowLong(hWnd, GWL_EXSTYLE) & WS_EX_LAYERED;
	bool layered = el > 0 ? true : false;
	BOOL visible = IsWindowVisible(hWnd);

	DWORD processId = 0;
	GetWindowThreadProcessId(hWnd, &processId);

	ProcessInfo pi;
	get_process_info(processId, pi);

	_stprintf(szBuf, 
		_T("�����0x%.8X\r\n"
			"�ı���%s\r\n"
			"������%s\r\n"
			"λ�ã�%ld,%ld,%ld,%ld\r\n"
			"Layered: %d\r\n"
			"Visible: %d\r\n"
			"PID: %ld\r\n"
			"Exe: %s\r\n"
			"Desc: %s\r\n"
		), 
		hWnd, szText, szClass, 
		rc.left, rc.top, rc.right, rc.bottom, 
		layered, visible, 
		processId, pi.name.c_str(), pi.modules[0].desc.c_str());
	//����Ϣ��ʾ��������
	SetDlgItemText(g_hDlg, IDC_EDT_DETAIL, szBuf);
}
//����Ļ����ת��Ϊ��������
void ScreenToClientRect(LPRECT prc)
{
	ScreenToClient(g_hDlg, (LPPOINT)prc);
	ScreenToClient(g_hDlg, ((LPPOINT)prc) + 1);	
}
//����������ת��Ϊ��Ļ����
void ClientToScreenRect(LPRECT prc)
{
	ClientToScreen(g_hDlg, (LPPOINT)prc);
	ClientToScreen(g_hDlg, ((LPPOINT)prc) + 1);		
}

//ȡ�ô����б�
void GetWindowList(HWND hWnd)
{
	//�����
	TreeView_DeleteAllItems(g_hTree);
	//�Ƿ��ȡ����ϵͳ�д���
	if(hWnd == NULL)
		hWnd = GetDesktopWindow();
	//�ݹ��ȡ���д���
	GetListFunc(hWnd, TVI_ROOT);
	//�õ����ĸ��ڵ�
	HTREEITEM hRoot = TreeView_GetRoot(g_hTree);
	//ѡ�и��ڵ�
	if(hRoot != NULL)
		TreeView_SelectItem(g_hTree, hRoot);
}

//��ȡ�������Ӵ��ڵĵݹ麯��
void GetListFunc(HWND hWnd, HTREEITEM hItem)
{
	//�õ������ı�
	TCHAR szBuf[MAX_PATH] = {0};
	GetWindowText(hWnd, szBuf, MAX_PATH);
	//����ı�Ϊ�գ���ȡ������
	if(*szBuf == _T('\0'))
	{
		*szBuf = _T('[');
		GetClassName(hWnd, szBuf+1, MAX_PATH - 2);
		_tcscat(szBuf, _T("]"));
	}

	BOOL layered = ( GetWindowLong(hWnd, GWL_EXSTYLE) & WS_EX_LAYERED ) > 0 ? TRUE : FALSE;
	BOOL visible = IsWindowVisible(hWnd);
	if( (hWnd != GetDesktopWindow()) && ( (g_showLayered && !layered) || (g_showVisible && !visible ) ))
	{}
	else
	{
		//����ڵ�
		TV_INSERTSTRUCT tis = {0};
		tis.hInsertAfter	= TVI_LAST;
		tis.hParent			= hItem;
		tis.item.mask		= TVIF_TEXT | TVIF_PARAM;
		tis.item.pszText	= szBuf;
		tis.item.lParam		= (LPARAM)hWnd;
		HTREEITEM hChildItem = TreeView_InsertItem(g_hTree, &tis);
		//����ö��
		EnumChildWindows(hWnd, EnumFunc, (LPARAM)hChildItem);
	}
}
//ö�ٴ��ڵĻص�����
BOOL CALLBACK EnumFunc(HWND hwnd, LPARAM lParam)
{
	GetListFunc(hwnd, (HTREEITEM)lParam);
	return TRUE;
}


