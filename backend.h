/*
 * Structures and prototypes dealing with the backend
 * plugins.
 */

#ifndef _BACKEND_H
#define _BACKEND_H

#define GENERAL_NAME_MAX         30
#define MAIL_ITEM_MAX            256
#define PLUGIN_MAX               10

enum {
	DATA_TYPE_NONE,
	DATA_TYPE_ACCOUNT,
	DATA_TYPE_FOLDER,
	DATA_TYPE_ITEM,
	DATA_TYPE_SETTINGS
};

typedef void Account;

typedef struct {
	char           Name[GENERAL_NAME_MAX+1];
	char           Author[GENERAL_NAME_MAX+1];
} BackendInfo;

typedef struct {
	int            Type;
	Account*       Acc;
	char           Name[GENERAL_NAME_MAX+1];
	char           Parent[GENERAL_NAME_MAX+1];
	unsigned long  Flags;
	unsigned long  LastId;
} MailFolder;

typedef struct {
	int            Type;
	Account*       Acc;
	MailFolder*    Folder;
	unsigned long  Id;
	CDATE          Date;
	CTIME          Time;
	char           Topic[MAIL_ITEM_MAX+1];
	char           To[MAIL_ITEM_MAX+1];
	char           From[MAIL_ITEM_MAX+1];
	unsigned long  Size;
} MailItem;

enum {
	HOST_TYPE_NONE,
	HOST_TYPE_POP3,
	HOST_TYPE_SMTP
};

#define HOSTNAME_MAX    100
#define REALNAME_MAX    100
#define ACCOUNTNAME_MAX 100
#define USERNAME_MAX    32

typedef struct {
	int            Type;
	char           AccountName[ACCOUNTNAME_MAX+1];
	int            RecvHostType;
	char           RecvHostName[HOSTNAME_MAX+1];
	int            RecvHostPort;
	char           RecvHostUser[USERNAME_MAX+1];
	char           RecvHostPass[USERNAME_MAX+1];
	int            SendHostType;
	char           SendHostName[HOSTNAME_MAX+1];
	int            SendHostPort;
	char           SendHostUser[USERNAME_MAX+1];
	char           SendHostPass[USERNAME_MAX+1];
	char           UserEmail[HOSTNAME_MAX + USERNAME_MAX + 1];
	char           UserRealName[REALNAME_MAX+1];
	char           ReplyEmail[HOSTNAME_MAX + USERNAME_MAX + 1];
	char           ReplyRealName[REALNAME_MAX+1];
} AccountSettings;

/* Avoid horrible compiler warnings with Microsoft
 * Visual C.
 */
#ifdef MSVC
#define DAPI
#else
#define DAPI API
#endif

typedef struct {
	int            Type;
	HMOD           hMod;
	int            (* DAPI init)(BackendInfo *);
	int            (* DAPI shutdown)(void);
	Account*       (* DAPI openaccount)(char *);
	Account*       (* DAPI newaccount)(char *);
	int            (* DAPI closeaccount)(Account *);
	char**         (* DAPI getaccounts)(void);
	void           (* DAPI freeaccounts)(char **);
	void           (* DAPI newsettings)(Account *, AccountSettings *);
	void           (* DAPI getsettings)(Account *, AccountSettings *);
	MailFolder*    (* DAPI getfolders)(Account *);
	int            (* DAPI newfolder)(Account *, MailFolder *);
	int            (* DAPI delfolder)(Account *, MailFolder *);
	MailItem*      (* DAPI getitems)(Account *, MailFolder *);
	int            (* DAPI newitem)(Account *, MailFolder *, MailItem *);
	int            (* DAPI delitem)(Account *, MailFolder *, MailItem *);
	char *         (* DAPI getmail)(Account *, MailFolder *, MailItem *, unsigned long *);
	int            (* DAPI newmail)(Account *, MailFolder *, MailItem *, char *, unsigned long);
	void           (* DAPI free)(Account *, void *);
	BackendInfo    Info;
} BackendPlugin;


int load_backend(char *name);

#endif
