/*----------------------------------------------------------
OpenKey - The Cross platform Open source Vietnamese Keyboard application.

Copyright (C) 2019 Mai Vu Tuyen
Contact: maivutuyen.91@gmail.com
Github: https://github.com/tuyenvm/OpenKey
Fanpage: https://www.facebook.com/OpenKeyVN

This file is belong to the OpenKey project, Win32 version
which is released under GPL license.
You can fork, modify, improve this program. If you
redistribute your new version, it MUST be open source.
-----------------------------------------------------------*/
#include "stdafx.h"
#include "AppDelegate.h"

#define MASK_SHIFT				0x01
#define MASK_CONTROL			0x02
#define MASK_ALT				0x04
#define MASK_CAPITAL			0x08
#define MASK_NUMLOCK			0x10
#define MASK_WIN				0x20
#define MASK_SCROLL				0x40

#define OTHER_CONTROL_KEY (_flag & MASK_ALT) || (_flag & MASK_CONTROL)
#define DYNA_DATA(macro, pos) (macro ? pData->macroData[pos] : pData->charData[pos])
#define EMPTY_HOTKEY 0xFE0000FE

extern int vSendKeyStepByStep;
extern int vUseGrayIcon;
extern int vShowOnStartUp;
extern int vRunWithWindows;

static HHOOK hKeyboardHook;
static HHOOK hMouseHook;
static KBDLLHOOKSTRUCT* keyboardData;
static MSLLHOOKSTRUCT* mouseData;
static vKeyHookState* pData;
static vector<Uint16> _syncKey;
static Uint32 _flag = 0, _lastFlag = 0, _privateFlag;
static bool _flagChanged = false, _isFlagKey;
static Uint16 _keycode;
static Uint16 _newChar, _newCharHi;

static vector<Uint16> _newCharString;
static Uint16 _newCharSize;
static bool _willSendControlKey = false;

static Uint16 _uniChar[2];
static int _i, _j, _k;
static Uint32 _tempChar;

static string macroText, macroContent;
static int _languageTemp = 0; //use for smart switch key
static vector<Byte> savedSmartSwitchKeyData; ////use for smart switch key

static INPUT backspaceEvent[2];
static INPUT keyEvent[2];

LRESULT CALLBACK keyboardHookProcess(int nCode, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK mouseHookProcess(int nCode, WPARAM wParam, LPARAM lParam);

void OpenKeyFree() {
	UnhookWindowsHookEx(hMouseHook);
	UnhookWindowsHookEx(hKeyboardHook);
}

void OpenKeyInit() {
	pData = (vKeyHookState*)vKeyInit();

	//pre-create back key
	backspaceEvent[0].type = INPUT_KEYBOARD;
	backspaceEvent[0].ki.dwFlags = 0;
	backspaceEvent[0].ki.wVk = VK_BACK;
	backspaceEvent[0].ki.wScan = 0;
	backspaceEvent[0].ki.time = 0;
	backspaceEvent[0].ki.dwExtraInfo = 1;

	backspaceEvent[1].type = INPUT_KEYBOARD;
	backspaceEvent[1].ki.dwFlags = KEYEVENTF_KEYUP;
	backspaceEvent[1].ki.wVk = VK_BACK;
	backspaceEvent[1].ki.wScan = 0;
	backspaceEvent[1].ki.time = 0;
	backspaceEvent[1].ki.dwExtraInfo = 1;

	//get key state
	_flag = 0;
	if (GetKeyState(VK_LSHIFT) < 0 || GetKeyState(VK_RSHIFT) < 0) _flag |= MASK_SHIFT;
	if (GetKeyState(VK_LCONTROL) < 0 || GetKeyState(VK_RCONTROL) < 0) _flag |= MASK_CONTROL;
	if (GetKeyState(VK_LMENU) < 0 || GetKeyState(VK_RMENU) < 0) _flag |= MASK_ALT;
	if (GetKeyState(VK_LWIN) < 0 || GetKeyState(VK_RWIN) < 0) _flag |= MASK_WIN;
	if (GetKeyState(VK_NUMLOCK) < 0) _flag |= MASK_NUMLOCK;
	if (GetKeyState(VK_CAPITAL) == 1) _flag |= MASK_CAPITAL;
	if (GetKeyState(VK_SCROLL) < 0) _flag |= MASK_SCROLL;

	//init and load macro data
	DWORD macroDataSize;
	BYTE* macroData = OpenKeyHelper::getRegBinary(_T("macroData"), macroDataSize);
	initMacroMap((Byte*)macroData, (int)macroDataSize);

	//init and load smart switch key data
	DWORD smartSwitchKeySize;
	BYTE* data = OpenKeyHelper::getRegBinary(_T("smartSwitchKey"), smartSwitchKeySize);
	initSmartSwitchKey((Byte*)data, (int)smartSwitchKeySize);

	APP_GET_DATA(vLanguage, 1);
	APP_GET_DATA(vInputType, 0);
	vFreeMark = 0;
	APP_GET_DATA(vCodeTable, 0);
	APP_GET_DATA(vCheckSpelling, 1);
	APP_GET_DATA(vUseModernOrthography, 0);
	APP_GET_DATA(vQuickTelex, 0);
	APP_GET_DATA(vSwitchKeyStatus, 0x7A000206);
	APP_GET_DATA(vRestoreIfWrongSpelling, 1);
	APP_GET_DATA(vFixRecommendBrowser, 1);
	APP_GET_DATA(vUseMacro, 1);
	APP_GET_DATA(vUseMacroInEnglishMode, 0);
	APP_GET_DATA(vSendKeyStepByStep, 1);
	APP_GET_DATA(vUseGrayIcon, 0);
	APP_GET_DATA(vShowOnStartUp, 1);
	APP_GET_DATA(vRunWithWindows, 1);
	OpenKeyHelper::registerRunOnStartup(vRunWithWindows);
	APP_GET_DATA(vUseSmartSwitchKey, 1);
	APP_GET_DATA(vUpperCaseFirstChar, 0);

	//init convert tool
	APP_GET_DATA(convertToolDontAlertWhenCompleted, 0);
	APP_GET_DATA(convertToolToAllCaps, 0);
	APP_GET_DATA(convertToolToAllNonCaps, 0);
	APP_GET_DATA(convertToolToCapsFirstLetter, 0);
	APP_GET_DATA(convertToolToCapsEachWord, 0);
	APP_GET_DATA(convertToolRemoveMark, 0);
	APP_GET_DATA(convertToolFromCode, 0);
	APP_GET_DATA(convertToolToCode, 0);
	APP_GET_DATA(convertToolHotKey, EMPTY_HOTKEY);
	if (convertToolHotKey == 0) {
		convertToolHotKey = EMPTY_HOTKEY;
	}

	//init hook
	HINSTANCE hInstance = GetModuleHandle(NULL);
	hKeyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, keyboardHookProcess, hInstance, 0);
	hMouseHook = SetWindowsHookEx(WH_MOUSE_LL, mouseHookProcess, hInstance, 0);
}

void saveSmartSwitchKeyData() {
	getSmartSwitchKeySaveData(savedSmartSwitchKeyData);
	OpenKeyHelper::setRegBinary(_T("smartSwitchKey"), savedSmartSwitchKeyData.data(), (int)savedSmartSwitchKeyData.size());
}

static void InsertKeyLength(const Uint8& len) {
	_syncKey.push_back(len);
}

static inline void prepareKeyEvent(INPUT& input, const Uint16& keycode, const bool& isPress) {
	input.type = INPUT_KEYBOARD;
	input.ki.dwFlags = isPress ? 0 : KEYEVENTF_KEYUP;
	input.ki.wVk = keycode;
	input.ki.wScan = 0;
	input.ki.time = 0;
	input.ki.dwExtraInfo = 1;
}

static inline void prepareUnicodeEvent(INPUT& input, const Uint16& unicode, const bool& isPress) {
	input.type = INPUT_KEYBOARD;
	input.ki.wVk = 0;
	input.ki.wScan = unicode;
	input.ki.time = 0;
	input.ki.dwFlags = (isPress ? 0 : KEYEVENTF_KEYUP) | KEYEVENTF_UNICODE;
	input.ki.dwExtraInfo = 1;
}

static void SendCombineKey(const Uint16& key1, const Uint16& key2) {
	prepareKeyEvent(keyEvent[0], key1, true);
	SendInput(1, keyEvent, sizeof(INPUT));

	prepareKeyEvent(keyEvent[0], key2, true);
	prepareKeyEvent(keyEvent[1], key2, false);
	SendInput(2, keyEvent, sizeof(INPUT));

	prepareKeyEvent(keyEvent[0], key1, false);
	SendInput(1, keyEvent, sizeof(INPUT));
}

static void SendKeyCode(Uint32 data) {
	_newChar = (Uint16)data;
	if (_newChar < 128 || keyCodeToCharacter(data) != 0){
		if (IS_DOUBLE_CODE(vCodeTable)) //VNI
			InsertKeyLength(1);

		if (data & CAPS_MASK && !((_flag & MASK_SHIFT) || (_flag & MASK_CAPITAL))) {
			prepareKeyEvent(keyEvent[0], KEY_LEFT_SHIFT, true);
			SendInput(1, keyEvent, sizeof(INPUT));
		}
	
		prepareKeyEvent(keyEvent[0], _newChar, true);
		prepareKeyEvent(keyEvent[1], _newChar, false);
		SendInput(2, keyEvent, sizeof(INPUT));

		if (data & CAPS_MASK && !((_flag & MASK_SHIFT) || (_flag & MASK_CAPITAL))) {
			prepareKeyEvent(keyEvent[0], KEY_LEFT_SHIFT, false);
			SendInput(1, keyEvent, sizeof(INPUT));
		}
	} else {
		if (vCodeTable == 0) { //unicode 2 bytes code
			prepareUnicodeEvent(keyEvent[0], _newChar, true);
			prepareUnicodeEvent(keyEvent[1], _newChar, false);
			SendInput(2, keyEvent, sizeof(INPUT));
		} else if (vCodeTable == 1 || vCodeTable == 2 || vCodeTable == 4) { //others such as VNI Windows, TCVN3: 1 byte code
			_newCharHi = HIBYTE(_newChar);
			_newChar = LOBYTE(_newChar);

			prepareUnicodeEvent(keyEvent[0], _newChar, true);
			prepareUnicodeEvent(keyEvent[1], _newChar, false);
			SendInput(2, keyEvent, sizeof(INPUT));

			if (_newCharHi > 32) {
				if (vCodeTable == 2) //VNI
					InsertKeyLength(2);
				prepareUnicodeEvent(keyEvent[0], _newCharHi, true);
				prepareUnicodeEvent(keyEvent[1], _newCharHi, false);
				SendInput(2, keyEvent, sizeof(INPUT));
			} else {
				if (vCodeTable == 2) //VNI
					InsertKeyLength(1);
			}
		} else if (vCodeTable == 3) { //Unicode Compound
			_newCharHi = (_newChar >> 13);
			_newChar &= 0x1FFF;
			_uniChar[0] = _newChar;
			_uniChar[1] = _newCharHi > 0 ? (_unicodeCompoundMark[_newCharHi - 1]) : 0;
			InsertKeyLength(_newCharHi > 0 ? 2 : 1);
			prepareUnicodeEvent(keyEvent[0], _uniChar[0], true);
			prepareUnicodeEvent(keyEvent[1], _uniChar[0], false);
			SendInput(2, keyEvent, sizeof(INPUT));
			if (_newCharHi > 0) {
				prepareUnicodeEvent(keyEvent[0], _uniChar[1], true);
				prepareUnicodeEvent(keyEvent[1], _uniChar[1], false);
				SendInput(2, keyEvent, sizeof(INPUT));
			}
		}
	}
}

static void SendBackspace() {
	SendInput(2, backspaceEvent, sizeof(INPUT));
	if (IS_DOUBLE_CODE(vCodeTable)) { //VNI or Unicode Compound
		if (_syncKey.back() > 1) {
			/*if (!(vCodeTable == 3 && containUnicodeCompoundApp(FRONT_APP))) {
				SendInput(2, backspaceEvent, sizeof(INPUT));
			}*/
			SendInput(2, backspaceEvent, sizeof(INPUT));
		}
		_syncKey.pop_back();
	}
}

static void SendEmptyCharacter() {
	if (IS_DOUBLE_CODE(vCodeTable)) //VNI or Unicode Compound
		InsertKeyLength(1);

	_newChar = 0x202F; //empty char

	prepareUnicodeEvent(keyEvent[0], _newChar, true);
	prepareUnicodeEvent(keyEvent[1], _newChar, false);
	SendInput(2, keyEvent, sizeof(INPUT));
}

static void SendNewCharString(const bool& dataFromMacro = false) {
	_j = 0;
	_newCharSize = dataFromMacro ? (Uint16)pData->macroData.size() : pData->newCharCount;
	if (_newCharString.size() < _newCharSize) {
		_newCharString.resize(_newCharSize);
	}
	_willSendControlKey = false;
	
	if (_newCharSize > 0) {
		for (_k = dataFromMacro ? 0 : pData->newCharCount - 1;
			dataFromMacro ? _k < pData->macroData.size() : _k >= 0;
			dataFromMacro ? _k++ : _k--) {

			_tempChar = DYNA_DATA(dataFromMacro, _k);
			if (_tempChar & PURE_CHARACTER_MASK) {
				_newCharString[_j++] = _tempChar;
				if (IS_DOUBLE_CODE(vCodeTable)) {
					InsertKeyLength(1);
				}
			} else if (_tempChar < 128 || keyCodeToCharacter(_tempChar) != 0 || ((Uint16)_tempChar < 128 && (_tempChar & CAPS_MASK))) {
				if (IS_DOUBLE_CODE(vCodeTable)) //VNI
					InsertKeyLength(1);
				_newCharString[_j++] = keyCodeToCharacter(_tempChar);
			} else {
				if (vCodeTable == 0) {  //unicode 2 bytes code
					_newCharString[_j++] = _tempChar;
				} else if (vCodeTable == 1 || vCodeTable == 2 || vCodeTable == 4) { //others such as VNI Windows, TCVN3: 1 byte code
					_newChar = _tempChar;
					_newCharHi = HIBYTE(_newChar);
					_newChar = LOBYTE(_newChar);
					_newCharString[_j++] = _newChar;

					if (_newCharHi > 32) {
						if (vCodeTable == 2) //VNI
							InsertKeyLength(2);
						_newCharString[_j++] = _newCharHi;
						_newCharSize++;
					}
					else {
						if (vCodeTable == 2) //VNI
							InsertKeyLength(1);
					}
				} else if (vCodeTable == 3) { //Unicode Compound
					_newChar = _tempChar;
					_newCharHi = (_newChar >> 13);
					_newChar &= 0x1FFF;

					InsertKeyLength(_newCharHi > 0 ? 2 : 1);
					_newCharString[_j++] = _newChar;
					if (_newCharHi > 0) {
						_newCharSize++;
						_newCharString[_j++] = _unicodeCompoundMark[_newCharHi - 1];
					}

				}
			}
		}//end for
	}

	if (pData->code == vRestore || pData->code == vRestoreAndStartNewSession) { //if is restore
		if (keyCodeToCharacter(_keycode) != 0) {
			_newCharSize++;
			_newCharString[_j++] = keyCodeToCharacter(_keycode | ((_flag & MASK_SHIFT) || (_flag & MASK_CAPITAL) ? CAPS_MASK : 0));
		} else {
			_willSendControlKey = true;
		}
	}
	if (pData->code == vRestoreAndStartNewSession) {
		startNewSession();
	}

	OpenKeyHelper::setClipboardText((LPCTSTR)_newCharString.data(), _newCharSize + 1, CF_UNICODETEXT);
	SendCombineKey(VK_LCONTROL, KEY_V);

	//the case when hCode is vRestore or vRestoreAndStartNewSession,
	//the word is invalid and last key is control key such as TAB, LEFT ARROW, RIGHT ARROW,...
	if (_willSendControlKey) {
		SendKeyCode(_keycode);
	}
}

bool checkHotKey(int hotKeyData, bool checkKeyCode = true) {
	if ((hotKeyData & (~0x8000)) == EMPTY_HOTKEY)
		return false;
	if (HAS_CONTROL(hotKeyData) ^ GET_BOOL(_lastFlag & MASK_CONTROL))
		return false;
	if (HAS_OPTION(hotKeyData) ^ GET_BOOL(_lastFlag & MASK_ALT))
		return false;
	if (HAS_COMMAND(hotKeyData) ^ GET_BOOL(_lastFlag & MASK_WIN))
		return false;
	if (HAS_SHIFT(hotKeyData) ^ GET_BOOL(_lastFlag & MASK_SHIFT))
		return false;
	if (checkKeyCode) {
		if (GET_SWITCH_KEY(hotKeyData) != _keycode)
			return false;
	}
	return true;
}

void switchLanguage() {
	if (vLanguage == 0)
		vLanguage = 1;
	else
		vLanguage = 0;
	if (HAS_BEEP(vSwitchKeyStatus))
		MessageBeep(MB_OK);
	AppDelegate::getInstance()->onInputMethodChangedFromHotKey();
	startNewSession();
	if (vUseSmartSwitchKey) {
		setAppInputMethodStatus(OpenKeyHelper::getFrontMostAppExecuteName(), vLanguage);
		saveSmartSwitchKeyData();
	}
}

static void SendPureCharacter(const Uint16& ch) {
	if (ch < 128)
		SendKeyCode(ch);
	else {
		prepareUnicodeEvent(keyEvent[0], ch, true);
		prepareUnicodeEvent(keyEvent[1], ch, false);
		SendInput(2, keyEvent, sizeof(INPUT));
		if (IS_DOUBLE_CODE(vCodeTable)) {
			InsertKeyLength(1);
		}
	}
}

static void handleMacro() {
	//fix autocomplete
	if (vFixRecommendBrowser) {
		SendEmptyCharacter();
		pData->backspaceCount++;
	}

	//send backspace
	if (pData->backspaceCount > 0) {
		for (int i = 0; i < pData->backspaceCount; i++) {
			SendBackspace();
		}
	}
	//send real data
	if (!vSendKeyStepByStep) {
		SendNewCharString(true);
	} else {
		for (int i = 0; i < pData->macroData.size(); i++) {
			if (pData->macroData[i] & PURE_CHARACTER_MASK) {
				SendPureCharacter(pData->macroData[i]);
			} else {
				SendKeyCode(pData->macroData[i]);
			}
		}
	}
	SendPureCharacter(' ');
}

static bool SetModifierMask(const Uint16& vkCode) {
	if (vkCode == VK_LSHIFT || vkCode == VK_RSHIFT) _flag |= MASK_SHIFT;
	else if (vkCode == VK_LCONTROL || vkCode == VK_RCONTROL) _flag |= MASK_CONTROL;
	else if (vkCode == VK_LMENU || vkCode == VK_RMENU) _flag |= MASK_ALT;
	else if (vkCode == VK_LWIN || vkCode == VK_RWIN) _flag |= MASK_WIN;
	else if (vkCode == VK_NUMLOCK) _flag |= MASK_NUMLOCK;
	else if (vkCode == VK_CAPITAL) _flag ^= MASK_CAPITAL;
	else if (vkCode == VK_SCROLL) _flag |= MASK_SCROLL;
	else { 
		_isFlagKey = false;
		return false; 
	}
	_isFlagKey = true;
	return true;
}

static bool UnsetModifierMask(const Uint16& vkCode) {
	if (vkCode == VK_LSHIFT || vkCode == VK_RSHIFT) _flag ^= MASK_SHIFT;
	else if (vkCode == VK_LCONTROL || vkCode == VK_RCONTROL) _flag ^= MASK_CONTROL;
	else if (vkCode == VK_LMENU || vkCode == VK_RMENU) _flag ^= MASK_ALT;
	else if (vkCode == VK_LWIN || vkCode == VK_RWIN) _flag ^= MASK_WIN;
	else if (vkCode == VK_NUMLOCK) _flag ^= MASK_NUMLOCK;
	else if (vkCode == VK_SCROLL) _flag ^= MASK_SCROLL;
	else { 
		_isFlagKey = false;
		return false; 
	}
	_isFlagKey = true;
	return true;
}

LRESULT CALLBACK keyboardHookProcess(int nCode, WPARAM wParam, LPARAM lParam) {
	keyboardData = (KBDLLHOOKSTRUCT *)lParam;
	//ignore my event
	if (keyboardData->dwExtraInfo != 0) {
		return CallNextHookEx(hKeyboardHook, nCode, wParam, lParam);
	}

	//check modifier key
	if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
		//LOG(L"Key down: %d\n", keyboardData->vkCode);
		SetModifierMask((Uint16)keyboardData->vkCode);
	} else if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP) {
		//LOG(L"Key up: %d\n", keyboardData->vkCode);
		UnsetModifierMask((Uint16)keyboardData->vkCode);
	}
	if (!_isFlagKey)
		_keycode = (Uint16)keyboardData->vkCode;

	//switch language shortcut; convert hotkey
	if (wParam == WM_KEYDOWN && !_isFlagKey && _keycode != 0) {
		if (GET_SWITCH_KEY(vSwitchKeyStatus) != _keycode && GET_SWITCH_KEY(convertToolHotKey) != _keycode) {
			_lastFlag = 0;
		} else {
			if (GET_SWITCH_KEY(vSwitchKeyStatus) == _keycode && checkHotKey(vSwitchKeyStatus, GET_SWITCH_KEY(vSwitchKeyStatus) != 0xFE)) {
				switchLanguage();
				_lastFlag = 0;
				_keycode = 0;
				return -1;
			}
			if (GET_SWITCH_KEY(convertToolHotKey) == _keycode && checkHotKey(convertToolHotKey, GET_SWITCH_KEY(convertToolHotKey) != 0xFE)) {
				AppDelegate::getInstance()->onQuickConvert();
				_lastFlag = 0;
				_keycode = 0;
				return -1;
			}
		}
	} else if (_isFlagKey) {
		if (_lastFlag == 0 || _lastFlag < _flag)
			_lastFlag = _flag;
		else if (_lastFlag > _flag) {
			//check switch
			if (checkHotKey(vSwitchKeyStatus, GET_SWITCH_KEY(vSwitchKeyStatus) != 0xFE)) {
				switchLanguage();
			}
			if (checkHotKey(convertToolHotKey, GET_SWITCH_KEY(convertToolHotKey) != 0xFE)) {
				AppDelegate::getInstance()->onQuickConvert();
			}
			_lastFlag = 0;
		}
		_keycode = 0;
		return CallNextHookEx(hKeyboardHook, nCode, wParam, lParam);
	}

	//smart switch key
	if (vUseSmartSwitchKey && wParam == WM_KEYDOWN) {
		string& exe = OpenKeyHelper::getFrontMostAppExecuteName();
		_languageTemp = getAppInputMethodStatus(exe, vLanguage);
		if (_languageTemp != vLanguage) {
			if (_languageTemp != -1) {
				vLanguage = _languageTemp;
				AppDelegate::getInstance()->onInputMethodChangedFromHotKey();
				startNewSession();
			} else {
				saveSmartSwitchKeyData();
			}
		}
	}

	//if is in english mode
	if (vLanguage == 0) {
		if (vUseMacro && vUseMacroInEnglishMode && wParam == WM_KEYDOWN) {
			vEnglishMode((wParam == WM_KEYDOWN ? vKeyEventState::KeyDown : vKeyEventState::MouseDown),
				_keycode,
				(_flag & MASK_SHIFT) || (_flag & MASK_CAPITAL),
				OTHER_CONTROL_KEY);

			if (pData->code == vReplaceMaro) { //handle macro in english mode
				handleMacro();
				return NULL;
			}
		}
		return CallNextHookEx(hKeyboardHook, nCode, wParam, lParam);
	}

	//handle keyboard
	if (wParam == WM_KEYDOWN) {
		//send event signal to Engine
		vKeyHandleEvent(vKeyEvent::Keyboard,
						vKeyEventState::KeyDown,
						_keycode,
						(_flag & MASK_SHIFT && _flag & MASK_CAPITAL) ? 0 : (_flag & MASK_SHIFT ? 1 : (_flag & MASK_CAPITAL ? 2 : 0)),
						OTHER_CONTROL_KEY);
		if (pData->code == vDoNothing) { //do nothing
			if (IS_DOUBLE_CODE(vCodeTable)) { //VNI
				if (pData->extCode == 1) { //break key
					_syncKey.clear();
				} else if (pData->extCode == 2) { //delete key
					if (_syncKey.size() > 0) {
						if (_syncKey.back() > 1 && (vCodeTable == 2 || vCodeTable == 3)) {
							//send one more backspace
							SendInput(2, backspaceEvent, sizeof(INPUT));
						}
						_syncKey.pop_back();
					}
				} else if (pData->extCode == 3) { //normal key
					InsertKeyLength(1);
				}
			}
			return CallNextHookEx(hKeyboardHook, nCode, wParam, lParam);
		} else if (pData->code == vWillProcess || pData->code == vRestore || pData->code == vRestoreAndStartNewSession) { //handle result signal
			//fix autocomplete
			if (vFixRecommendBrowser && pData->extCode != 4) {
				SendEmptyCharacter();
				pData->backspaceCount++;
			}

			//send backspace
			if (pData->backspaceCount > 0 && pData->backspaceCount < MAX_BUFF) {
				for (_i = 0; _i < pData->backspaceCount; _i++) {
					SendBackspace();
				}
			}

			//send new character
			if (!vSendKeyStepByStep) {
				SendNewCharString();
			} else {
				if (pData->newCharCount > 0 && pData->newCharCount <= MAX_BUFF) {
					for (int i = pData->newCharCount - 1; i >= 0; i--) {
						SendKeyCode(pData->charData[i]);
					}
				}
				if (pData->code == vRestore || pData->code == vRestoreAndStartNewSession) {
					SendKeyCode(_keycode | ((_flag & MASK_CAPITAL) || (_flag & MASK_SHIFT) ? CAPS_MASK : 0));
				}
				if (pData->code == vRestoreAndStartNewSession) {
					startNewSession();
				}
			}
		} else if (pData->code == vReplaceMaro) { //MACRO
			handleMacro();
		}
		return -1; //consume event
	}
	return CallNextHookEx(hKeyboardHook, nCode, wParam, lParam);
}

LRESULT CALLBACK mouseHookProcess(int nCode, WPARAM wParam, LPARAM lParam) {
	mouseData = (MSLLHOOKSTRUCT *)lParam;
	switch (wParam) {
	case WM_LBUTTONDOWN:
	
	case WM_RBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_XBUTTONDOWN:
	case WM_NCXBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_RBUTTONUP:
	case WM_MBUTTONUP:
	case WM_XBUTTONUP:
	case WM_NCXBUTTONUP:
		//send event signal to Engine
		vKeyHandleEvent(vKeyEvent::Mouse, vKeyEventState::MouseDown, 0);
		if (IS_DOUBLE_CODE(vCodeTable)) { //VNI
			_syncKey.clear();
		}
		break;
	}
	return CallNextHookEx(hMouseHook, nCode, wParam, lParam);
}