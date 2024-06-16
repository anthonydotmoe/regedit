#ifndef BASEWINDOW_H
#define BASEWINDOW_H

#include <windows.h>

class BaseWindow {
	public:
		BaseWindow();
		virtual ~BaseWindow() {}

		static LRESULT CALLBACK WindowProc(HWND, UINT, WPARAM, LPARAM);

		HRESULT Create(HINSTANCE hInstance);
		HRESULT Show(int nCmdShow);

		virtual LRESULT OnReceiveMessage(UINT msg, WPARAM wparam, LPARAM lparam);

	protected:

		HRESULT Register();

		virtual LPCTSTR ClassName() const = 0;
		virtual LPCTSTR MenuName() const { return NULL; }
		virtual LPCTSTR WindowName() const = 0;

		virtual void OnPaint() = 0;

		HWND 		m_hwnd;
		HINSTANCE	m_hInstance;
};

#endif
