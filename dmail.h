/* $Id: dmail.h,v 1.4 2005/06/08 15:53:52 crazybonz Exp $ */
#ifndef _DMAIL_H
#define _DMAIL_H

#define MAIN_FRAME    155
#define ID_FILEMENU   156
#define ID_EDITMENU   158

#define ID_TOOLBAR    1045
#define ID_LPANE      1046
#define ID_RPANE      1047
#define ID_NOTEBOOK   1048
#define ID_NOTEBOOKFR 1049
#define ID_STATUS     1050
#define ID_LIST       1051
#define ID_MLE        1052
#define ID_TOP        1053

#define PB_CHANGE 309

#define IDM_FILE        250
#define IDM_EXIT        251
#define IDM_HELP        252
#define IDM_GENERALHELP 253
#define IDM_ABOUT       254
#define IDM_PREFERENCES 255
#define IDM_MESSAGE     256
#define IDM_NEWMESS     257
#define IDM_SAVE        258
#define IDM_SEND        259
#define IDM_CHECK       260
#define IDM_SENDMAIL    261
#define IDM_DELITEM     262
#define IDM_RAWTOG      263
#define IDM_SAVEPOS     264
#define IDM_TOOLS       265
#define IDM_REPLY       266
#define IDM_REPLYQ      267
#define IDM_REPLYALL    268
#define IDM_REPLYALLQ   269
#define IDM_NEWFOLD     270
#define IDM_TRASH       271
#define IDM_ACCOUNT     272

#define CONNECT 335
#define DISCONNECT 336
#define ADDTOQ 337
#define REMOVEFROMQ 338
#define FLUSHQ 346
#define ADMIN 349
#define SAVETITLE 350
#define UNSAVETITLE 351
#define HOST_TITLE 352
#define REMOVETAB 353
#define PREFERENCES 354
#define NEWTAB 355
#define FILEICON 356
#define FOLDERICON 357
#define LINKICON 365

#define DLG_ABOUT 1202

#define TB_SEPARATOR -1

#define LOGO 1300

#define ACCOUNT_MAX   20

typedef struct {
	int                Type;
	Account*           Acc;
	int                Plug;
	MailFolder*        Folders;
	AccountSettings    Settings;
	HEV                SendEve, RecvEve;
} AccountInfo;

typedef struct {
	char *plugins[PLUGIN_MAX];
	char *accounts[ACCOUNT_MAX];
	unsigned long x, y, width, height;
	unsigned long hsplit, vsplit;
} DmailConfig;

/* Select an editor for the current build platform. */
#if defined(__OS2__) || defined(__EMX__)
#define EDITOR "epm"
#define EDMODE DW_EXEC_GUI
#elif defined(__WIN32__) || defined(WINNT)
#define EDITOR "notepad"
#define EDMODE DW_EXEC_GUI
#else
#define EDITOR "vi"
#define EDMODE DW_EXEC_CON
#endif

/* Prototypes */
int DWSIGNAL new_message(HWND hwnd, void *data);
int DWSIGNAL reply_message(HWND hwnd, void *data);
int DWSIGNAL replyq_message(HWND hwnd, void *data);
int DWSIGNAL replyall_message(HWND hwnd, void *data);
int DWSIGNAL replyallq_message(HWND hwnd, void *data);
int DWSIGNAL empty_trash(HWND hwnd, void *data);
int DWSIGNAL new_folder(HWND hwnd, void *data);
void saveconfig(void);
void loadconfig(void);
AccountSettings *findsettings(void *opaque);
AccountInfo *findaccount(void *opaque);

#endif

