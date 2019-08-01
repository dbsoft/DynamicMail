/*
 * This is a MySQL Server based implementation of the
 * mail backend.
 */
#include <stdio.h>
#include <sys/stat.h>
#include <string.h>
#include <dw.h>
#include <dwcompat.h>
#include <mysql.h>
#include "backend.h"
#include "mysqlplg.h"

#define TITLE "DynamicMail MySQL Plugin"

/* Choose a unique ID for this plugin */
#define MYSQL_BACKEND_ID 124

HMTX backend_mtx;
MYSQL dmail_db;
LoginStruct li;
char *tsformat = "%04d-%02d-%02d %02d:%02d:%02d";

int API backend_newfolder(Account *acc, MailFolder *mf);

#define display_mysql_error() _display_mysql_error(__FILE__, __LINE__)

void _display_mysql_error(char *file, int line)
{
	dw_messagebox(TITLE, DW_MB_OK | DW_MB_ERROR, "File: %s Line %d\n%s", file, line, mysql_error(&dmail_db));
}

/* Handle OK press on the login dialog. */
void DWSIGNAL login_ok(HWND window, void *data)
{
	DWDialog *dwwait = (DWDialog *)data;
	HWND entrywindow = (HWND)dwwait->data, name = NULL, pass = NULL, host = NULL;
	char *login_username, *login_password, *login_hostname;

	name = (HWND)dw_window_get_data(entrywindow, "name");
	host = (HWND)dw_window_get_data(entrywindow, "host");
	pass = (HWND)dw_window_get_data(entrywindow, "pass");

	login_hostname = dw_window_get_text(host);
	login_username = dw_window_get_text(name);
	login_password = dw_window_get_text(pass);

	if(login_hostname)
	{
		strcpy(li.hostname, login_hostname);
		dw_free(login_hostname);
	}
	if(login_username)
	{
		strcpy(li.username, login_username);
		dw_free(login_username);
	}
	if(login_password)
	{
		strcpy(li.password, login_password);
		dw_free(login_password);
	}

	/* Destroy the dialog */
	dw_window_destroy(entrywindow);
	dw_dialog_dismiss(dwwait, (void *)1);
}

/* Handle Cancel press on the login dialog. */
void DWSIGNAL login_cancel(HWND window, void *data)
{
	DWDialog *dwwait = (DWDialog *)data;
	HWND entrywindow = (HWND)dwwait->data;

	/* Destroy the dialog */
	dw_window_destroy(entrywindow);
	dw_dialog_dismiss(dwwait, (void *)0);
}

/* A dialog to get MySQL login information from the user */
int login_dialog(void)
{
	HWND entrywindow, mainbox, entryfield, passfield, hostfield, cancelbutton, okbutton, buttonbox, stext;
	ULONG style = DW_FCF_TITLEBAR | DW_FCF_SHELLPOSITION | DW_FCF_DLGBORDER;
	DWDialog *dwwait;

	entrywindow = dw_window_new(HWND_DESKTOP, "DynamicMail MySQL Login", style);

	mainbox = dw_box_new(BOXVERT, 10);

	dw_box_pack_start(entrywindow, mainbox, 0, 0, TRUE, TRUE, 0);

	/* Hostname */
	stext = dw_text_new("Hostname:", 0);

	dw_window_set_style(stext, DW_DT_VCENTER, DW_DT_VCENTER);

	dw_box_pack_start(mainbox, stext, 130, 20, TRUE, TRUE, 2);

	hostfield = dw_entryfield_new("localhost", 100L);

	dw_entryfield_set_limit(hostfield, 20);

	dw_box_pack_start(mainbox, hostfield, 130, 20, TRUE, TRUE, 4);

	dw_window_set_data(entrywindow, "host", (void *)hostfield);

	/* User ID */
	stext = dw_text_new("Username:", 0);

	dw_window_set_style(stext, DW_DT_VCENTER, DW_DT_VCENTER);

	dw_box_pack_start(mainbox, stext, 130, 20, TRUE, TRUE, 2);

	entryfield = dw_entryfield_new("", 100L);

	dw_entryfield_set_limit(entryfield, 20);

	dw_box_pack_start(mainbox, entryfield, 130, 20, TRUE, TRUE, 4);

	dw_window_set_data(entrywindow, "name", (void *)entryfield);

	/* Password */
	stext = dw_text_new("Password:", 0);

	dw_window_set_style(stext, DW_DT_VCENTER, DW_DT_VCENTER);

	dw_box_pack_start(mainbox, stext, 130, 20, TRUE, TRUE, 2);

	passfield = dw_entryfield_password_new("", 100L);

	dw_entryfield_set_limit(passfield, 159);

	dw_box_pack_start(mainbox, passfield, 130, 20, TRUE, TRUE, 4);

	dw_window_set_data(entrywindow, "pass", (void *)passfield);

	/* Buttons */
	buttonbox = dw_box_new(BOXHORZ, 10);

	dw_box_pack_start(mainbox, buttonbox, 0, 0, TRUE, FALSE, 0);

	okbutton = dw_button_new("Ok", 1001L);

	dw_box_pack_start(buttonbox, okbutton, 130, 30, TRUE, FALSE, 2);

	cancelbutton = dw_button_new("Cancel", 1002L);

	dw_box_pack_start(buttonbox, cancelbutton, 130, 30, TRUE, FALSE, 2);

	dw_window_default(entrywindow, entryfield);

	dw_window_click_default(hostfield, entryfield);

	dw_window_click_default(entryfield, passfield);

	dw_window_click_default(passfield, okbutton);

	dwwait = dw_dialog_new((void *)entrywindow);

	dw_signal_connect(okbutton, DW_SIGNAL_CLICKED, DW_SIGNAL_FUNC(login_ok), (void *)dwwait);
	dw_signal_connect(cancelbutton, DW_SIGNAL_CLICKED, DW_SIGNAL_FUNC(login_cancel), (void *)dwwait);

	dw_window_set_size(entrywindow, 300, 250);

	dw_window_show(entrywindow);

	return (int)dw_dialog_wait(dwwait);
}

int minversion(int major, int minor, int sub)
{
	char *version = mysql_get_server_info(&dmail_db);
	int smajor, sminor, ssub;

	sscanf(version, "%d.%d.%d", &smajor, &sminor, &ssub);
	
	if(smajor > major)
		return 1;

	if(smajor == major)
	{
		if(sminor > minor)
			return 1;

		if(sminor == minor)
		{
			if(ssub >= sub)
				return 1;
		}
	}
	return 0;
}

/* Initialize this backend */
int API backend_init(BackendInfo *bi)
{
	/* Get the MySQL login information from the user */
	if(!login_dialog())
		return 0;

	mysql_init(&dmail_db);
	if(!mysql_real_connect(&dmail_db, li.hostname, li.username, li.password, NULL, 0, NULL, 0))
	{
		display_mysql_error();
		return 0;
	}

	/* Select the database we want to use */
	if(mysql_select_db(&dmail_db, "DynamicMail"))
	{
		if(mysql_query(&dmail_db, MYSQL_DATABASE))
		{
			display_mysql_error();
			return 0;
		}
		if(mysql_select_db(&dmail_db, "DynamicMail"))
		{
			display_mysql_error();
			return 0;
		}
	}
	/* Check to see if our tables exist, if not try to create them */
	if(mysql_query(&dmail_db, "SELECT * FROM `Accounts`")
	   && (mysql_query(&dmail_db, MYSQL_TABLE1)
	   || mysql_query(&dmail_db, MYSQL_TABLE2)
	   || mysql_query(&dmail_db, MYSQL_TABLE3)
	   || mysql_query(&dmail_db, MYSQL_TABLE4)))
	{
		display_mysql_error();
		return 0;
	}
	else
	{
		MYSQL_RES *res=mysql_store_result(&dmail_db);

		if(res)
		{
			mysql_free_result(res); /* Release memory used to store results. */
		}
	}

	/* If the server is a pre 4.1 version of MySQL
	 * then use the old timestamp format.
	 */
	if(!minversion(4, 1, 0))
	{
		tsformat = "%04d%02d%02d%02d%02d%02d";
	}

	sprintf(bi->Name, "MySQL");
	sprintf(bi->Author, "Brian Smith");

	backend_mtx = dw_mutex_new();

	return MYSQL_BACKEND_ID;
}

/* Shutdown this backend, doing any cleanup */
int API backend_shutdown(void)
{
	dw_mutex_close(backend_mtx);

	return 0;
}

/* Open a mail account, allocating any needed resources */
Account * API backend_openaccount(char *name)
{
	AccountStruct *as = malloc(sizeof(AccountStruct));

	dw_mutex_lock(backend_mtx);

	as->Type = DATA_TYPE_ACCOUNT;
	as->name = strdup(name);

	dw_mutex_unlock(backend_mtx);
	return (Account *)as;
}

/* Open a mail account, allocating any needed resources */
Account * API backend_newaccount(char *name)
{
	AccountStruct *as = malloc(sizeof(AccountStruct));
	MailFolder mf;
	char buf[100];

	dw_mutex_lock(backend_mtx);

	sprintf(buf, "INSERT INTO `Accounts` ( `name` )\nVALUES ( \'%s\' )", name);
	if(mysql_query(&dmail_db, buf))
		display_mysql_error();

	as->Type = DATA_TYPE_ACCOUNT;
	as->name = strdup(name);

	/* Create the default folders */
	strcpy(mf.Parent, "");
	mf.Flags = 0;
	strcpy(mf.Name, "Drafts");
	backend_newfolder(as, &mf);
	strcpy(mf.Name, "Inbox");
	backend_newfolder(as, &mf);
	strcpy(mf.Name, "Outbox");
	backend_newfolder(as, &mf);
	strcpy(mf.Name, "Sent Mail");
	backend_newfolder(as, &mf);
	strcpy(mf.Name, "Trash");
	backend_newfolder(as, &mf);

	dw_mutex_unlock(backend_mtx);
	return (Account *)as;
}

/* Close an account previously opened with openaccount() */
int API backend_closeaccount(Account *acc)
{
	AccountStruct *as = (AccountStruct *)acc;

	dw_mutex_lock(backend_mtx);
	if(as)
	{
		if(as->name)
			free(as->name);
		free(as);
	}
	dw_mutex_unlock(backend_mtx);
	return 0;
}

/* Get a list of available accounts */
char ** API backend_getaccounts(void)
{
	int counter = 0;
	char **ret = NULL;
	MYSQL_RES *res;
	MYSQL_ROW row;
	
	dw_mutex_lock(backend_mtx);

	if(!mysql_query(&dmail_db, "SELECT * FROM `Accounts`") && (res=mysql_store_result(&dmail_db)))
	{
		int rows;

		row=mysql_fetch_row(res); /* Get a row from the results */

		rows = (int)mysql_num_rows(res);

		if(rows)
		{
			ret = calloc(rows+1, sizeof(char *));

			while(row)
			{
				ret[counter] = strdup(row[1]);
				counter++;
				row=mysql_fetch_row(res);
			}
		}

		mysql_free_result(res); /* Release memory used to store results. */
	}
	else
		display_mysql_error();

	dw_mutex_unlock(backend_mtx);
	return ret;
}

/* Free the allocated list of accounts */
void API backend_freeaccounts(char **list)
{
	if(list)
	{
		int z = 0;

		while(list[z])
		{
			free(list[z]);
			z++;
		}
		free(list);
	}
}

/* Save settings associated with an account */
void API backend_newsettings(Account *acc, AccountSettings *set)
{
	AccountStruct *as = (AccountStruct *)acc;
	char buf[100];

	if(!as || !set)
		return;

	dw_mutex_lock(backend_mtx);
	set->Type = DATA_TYPE_SETTINGS;

	sprintf(buf, "UPDATE `Accounts` SET recvhosttype=\'%d\' WHERE name=\'%s\'", set->RecvHostType, as->name);
	mysql_query(&dmail_db, buf);
	sprintf(buf, "UPDATE `Accounts` SET recvhostname=\'%s\' WHERE name=\'%s\'", set->RecvHostName, as->name);
	mysql_query(&dmail_db, buf);
	sprintf(buf, "UPDATE `Accounts` SET recvhostport=\'%d\' WHERE name=\'%s\'", set->RecvHostPort, as->name);
	mysql_query(&dmail_db, buf);
	sprintf(buf, "UPDATE `Accounts` SET recvhostuser=\'%s\' WHERE name=\'%s\'", set->RecvHostUser, as->name);
	mysql_query(&dmail_db, buf);
	sprintf(buf, "UPDATE `Accounts` SET recvhostpass=\'%s\' WHERE name=\'%s\'", set->RecvHostPass, as->name);
	mysql_query(&dmail_db, buf);
	sprintf(buf, "UPDATE `Accounts` SET sendhosttype=\'%d\' WHERE name=\'%s\'", set->SendHostType, as->name);
	mysql_query(&dmail_db, buf);
	sprintf(buf, "UPDATE `Accounts` SET sendhostname=\'%s\' WHERE name=\'%s\'", set->SendHostName, as->name);
	mysql_query(&dmail_db, buf);
	sprintf(buf, "UPDATE `Accounts` SET sendhostport=\'%d\' WHERE name=\'%s\'", set->SendHostPort, as->name);
	mysql_query(&dmail_db, buf);
	sprintf(buf, "UPDATE `Accounts` SET sendhostuser=\'%s\' WHERE name=\'%s\'", set->SendHostUser, as->name);
	mysql_query(&dmail_db, buf);
	sprintf(buf, "UPDATE `Accounts` SET sendhostpass=\'%s\' WHERE name=\'%s\'", set->SendHostPass, as->name);
	mysql_query(&dmail_db, buf);
	sprintf(buf, "UPDATE `Accounts` SET useremail=\'%s\' WHERE name=\'%s\'", set->UserEmail, as->name);
	mysql_query(&dmail_db, buf);
	sprintf(buf, "UPDATE `Accounts` SET userrealname=\'%s\' WHERE name=\'%s\'", set->UserRealName, as->name);
	mysql_query(&dmail_db, buf);
	sprintf(buf, "UPDATE `Accounts` SET replyemail=\'%s\' WHERE name=\'%s\'", set->ReplyEmail, as->name);
	mysql_query(&dmail_db, buf);
	sprintf(buf, "UPDATE `Accounts` SET replyrealname=\'%s\' WHERE name=\'%s\'", set->ReplyRealName, as->name);
	mysql_query(&dmail_db, buf);

	dw_mutex_unlock(backend_mtx);
	return;
}

/* Load settings associated with an account */
void API backend_getsettings(Account *acc, AccountSettings *set)
{
	AccountStruct *as = (AccountStruct *)acc;
	char buf[100];
	MYSQL_RES *res;
	MYSQL_ROW row;
	
	if(!as || !set)
		return;

	dw_mutex_lock(backend_mtx);

	sprintf(buf, "SELECT * FROM `Accounts` WHERE name=\'%s\'", as->name);

	if(!mysql_query(&dmail_db, buf) && (res=mysql_store_result(&dmail_db)))
	{ 
		row=mysql_fetch_row(res); /* Get a row from the results */

		if(row)
		{
			set->Type = DATA_TYPE_SETTINGS;

			strncpy(set->AccountName, as->name, ACCOUNTNAME_MAX);
			set->RecvHostType = atoi(row[2]);
			strncpy(set->RecvHostName, row[3], HOSTNAME_MAX);
			set->RecvHostPort = atoi(row[4]);
			strncpy(set->RecvHostUser, row[5], USERNAME_MAX);
			strncpy(set->RecvHostPass, row[6], USERNAME_MAX);
			set->SendHostType = atoi(row[7]);
			strncpy(set->SendHostName, row[8], HOSTNAME_MAX);
			set->SendHostPort = atoi(row[9]);
			strncpy(set->SendHostUser, row[10], USERNAME_MAX);
			strncpy(set->SendHostPass, row[11], USERNAME_MAX);
			strncpy(set->UserEmail, row[12], HOSTNAME_MAX + USERNAME_MAX);
			strncpy(set->UserRealName, row[13], REALNAME_MAX);
			strncpy(set->ReplyEmail, row[14], HOSTNAME_MAX + USERNAME_MAX);
			strncpy(set->ReplyRealName, row[15], REALNAME_MAX);
		}
		mysql_free_result(res); /* Release memory used to store results. */
	}
	else
		display_mysql_error();

	dw_mutex_unlock(backend_mtx);
	return;
}

/* Get an array of folders */
MailFolder * API backend_getfolders(Account *acc)
{
	AccountStruct *as = (AccountStruct *)acc;
	MailFolder *mf = NULL;
	int counter = 0;
	char buf[100];
	MYSQL_RES *res;
	MYSQL_ROW row;
	
	if(!as)
		return NULL;

	dw_mutex_lock(backend_mtx);

	sprintf(buf, "SELECT * FROM `Folders` WHERE name=\'%s\' ORDER BY `folder`", as->name);

	if(!mysql_query(&dmail_db, buf) && (res=mysql_store_result(&dmail_db)))
	{
		int rows;

		row=mysql_fetch_row(res); /* Get a row from the results */

		rows = (int)mysql_num_rows(res);

		if(rows)
		{
			/* Allocate a sufficient return buffer */
			mf = calloc(rows + 1, sizeof(MailFolder));

			while(row)
			{
				mf[counter].Type = DATA_TYPE_FOLDER;
				mf[counter].Acc = acc;
				mf[counter].LastId = atoi(row[0]);
				strcpy(mf[counter].Name, row[2]);
				strcpy(mf[counter].Parent, row[3]);
				mf[counter].Flags = atoi(row[4]);
				counter++;
				row=mysql_fetch_row(res);
			}
		}
		mysql_free_result(res); /* Release memory used to store results. */
	}
	else
		display_mysql_error();

	dw_mutex_unlock(backend_mtx);
	return mf;
}

/* Create a new folder in an account */
int API backend_newfolder(Account *acc, MailFolder *mf)
{
	AccountStruct *as = (AccountStruct *)acc;
	char buf[200];

	if(!as || !mf)
		return 1;

	dw_mutex_lock(backend_mtx);

	sprintf(buf, "INSERT INTO `Folders` ( `name`, `folder`, `parent`, `flags` )\n" \
			"VALUES ( \'%s\', \'%s\', \'%s\', \'%d\' )", as->name, mf->Name, mf->Parent, (int)mf->Flags);
	if(mysql_query(&dmail_db, buf))
		display_mysql_error();

	mf->Type = DATA_TYPE_FOLDER;
	mf->Acc = acc;
	mf->LastId = (int)mysql_insert_id(&dmail_db);

	dw_mutex_unlock(backend_mtx);
	return 0;
}

/* Remove a folder from an account */
int API backend_delfolder(Account *acc, MailFolder *mf)
{
	AccountStruct *as = (AccountStruct *)acc;
	char buf[100];

	if(!as || !mf)
		return 1;

	dw_mutex_lock(backend_mtx);

	sprintf(buf, "DELETE FROM `Folders` WHERE entry=\'%d\'", (int)mf->LastId);
	if(mysql_query(&dmail_db, buf))
		display_mysql_error();

	dw_mutex_unlock(backend_mtx);
	return 0;
}

/* Get a list of mail entries */
MailItem * API backend_getitems(Account *acc, MailFolder *mf)
{
	AccountStruct *as = (AccountStruct *)acc;
	MailItem *mi = NULL;
	int counter = 0;
	char buf[100];
	MYSQL_RES *res;
	MYSQL_ROW row;
	
	if(!as || !mf)
		return NULL;

	dw_mutex_lock(backend_mtx);

	sprintf(buf, "SELECT * FROM `Mail` WHERE folder=\'%d\'", (int)mf->LastId);

	if(!mysql_query(&dmail_db, buf) && (res=mysql_store_result(&dmail_db)))
	{
		int rows;

		row=mysql_fetch_row(res); /* Get a row from the results */

		rows = (int)mysql_num_rows(res);

		if(rows)
		{
			/* Allocate a sufficient return buffer */
			mi = calloc(rows + 1, sizeof(MailItem));

			while(row)
			{
				int month = 0, day = 0, year = 0;
				int hours = 0, minutes = 0, seconds = 0;

				mi[counter].Type = DATA_TYPE_ITEM;
				mi[counter].Acc = acc;
				mi[counter].Folder = mf;
				mi[counter].Id = atoi(row[0]);
				mi[counter].Size = atoi(row[1]);
				sscanf(row[2], tsformat, &year, &month, &day, &hours, &minutes, &seconds);
				mi[counter].Date.month = month;
				mi[counter].Date.day = day;
				mi[counter].Date.year = year;
				mi[counter].Time.hours = hours;
				mi[counter].Time.minutes = minutes;
				mi[counter].Time.seconds = seconds;
				strcpy(mi[counter].Topic, row[3]);
				strcpy(mi[counter].To, row[4]);
				strcpy(mi[counter].From, row[5]);
				counter++;
				row=mysql_fetch_row(res);
			}
		}
		mysql_free_result(res); /* Release memory used to store results. */
	}
	else
		display_mysql_error();

	dw_mutex_unlock(backend_mtx);
	return mi;
}

/* Create a new mail item */
int API backend_newitem(Account *acc, MailFolder *mf, MailItem *mi)
{
	AccountStruct *as = (AccountStruct *)acc;
	char buf[300], tsbuf[20], *to, *from, *topic;

	if(!as || !mf || !mi)
		return 1;

	dw_mutex_lock(backend_mtx);

	to = malloc((strlen(mi->To)*2)+1);
	mysql_real_escape_string(&dmail_db, to, mi->To, strlen(mi->To));
	from = malloc((strlen(mi->From)*2)+1);
	mysql_real_escape_string(&dmail_db, from, mi->From, strlen(mi->From));
	topic = malloc((strlen(mi->Topic)*2)+1);
	mysql_real_escape_string(&dmail_db, topic, mi->Topic, strlen(mi->Topic));

	sprintf(tsbuf, tsformat, mi->Date.year, mi->Date.month, mi->Date.day, mi->Time.hours, mi->Time.minutes, mi->Time.seconds);
	sprintf(buf, "INSERT INTO `Mail` ( `size`, `datetime`, `topic`, `to`, `from`, `folder` )\n" \
			"VALUES ( \'%d\', \'%s\', \'%s\', \'%s\', \'%s\', \'%d\' )",
			(int)mi->Size, tsbuf, topic, to, from, (int)mf->LastId);
	if(mysql_query(&dmail_db, buf))
		display_mysql_error();

	if(to)
		free(to);
	if(from)
		free(from);
	if(topic)
		free(topic);

	mi->Id = mysql_insert_id(&dmail_db);
	mi->Type = DATA_TYPE_ITEM;
	mi->Acc = acc;
	mi->Folder = mf;

	dw_mutex_unlock(backend_mtx);

	return 0;
}

/* Remove a mail item from a mail folder */
int API backend_delitem(Account *acc, MailFolder *mf, MailItem *mi)
{
	AccountStruct *as = (AccountStruct *)acc;
	char buf[100];

	if(!as || !mf || !mi)
		return 1;

	dw_mutex_lock(backend_mtx);

	sprintf(buf, "DELETE FROM `Mail` WHERE entry=\'%d\'", (int)mi->Id);
	if(mysql_query(&dmail_db, buf))
		display_mysql_error();

	sprintf(buf, "DELETE FROM `Body` WHERE messid=\'%d\'", (int)mi->Id);
	if(mysql_query(&dmail_db, buf))
		display_mysql_error();

	dw_mutex_unlock(backend_mtx);

	return 0;
}

/* Retrieve a raw mail buffer associated with a mail item */
char * API backend_getmail(Account *acc, MailFolder *mf, MailItem *mi, unsigned long *len)
{
	AccountStruct *as = (AccountStruct *)acc;
	char buf[100], *mailbuf = NULL;
	MYSQL_RES *res;
	MYSQL_ROW row;
	
	if(!as || !mf || !mi || !len)
		return NULL;

	dw_mutex_lock(backend_mtx);

	sprintf(buf, "SELECT * FROM `Body` WHERE messid=\'%d\' ORDER BY `entry`", (int)mi->Id);

	if(!mysql_query(&dmail_db, buf) && (res=mysql_store_result(&dmail_db)))
	{ 
		row=mysql_fetch_row(res); /* Get a row from the results */

		if(row)
		{
			unsigned long *lengths = mysql_fetch_lengths(res);

			mailbuf = strdup(row[2]);

			if(lengths)
			{
				*len = lengths[2];
			}
			else
			{
				*len = mi->Size;
			}
		}
		mysql_free_result(res); /* Release memory used to store results. */
	}
	else
		display_mysql_error();

	dw_mutex_unlock(backend_mtx);
	return mailbuf;
}

/* Create a raw mail entry associated with a mail item */
int API backend_newmail(Account *acc, MailFolder *mf, MailItem *mi, char *buf, unsigned long len)
{
	AccountStruct *as = (AccountStruct *)acc;
	char *qbuf, *mbuf = malloc(len*2);
	int slen;

	if(!as || !mf || !mi || !mbuf)
		return 1;

	dw_mutex_lock(backend_mtx);

	slen = mysql_real_escape_string(&dmail_db, mbuf, buf, len);
	qbuf = malloc(slen + 200);

	if(qbuf)
	{
		sprintf(qbuf, "INSERT INTO `Body` ( `messid`, `body` )\nVALUES ( \'%d\' ,\'%s\' )", (int)mi->Id, mbuf);
		if(mysql_query(&dmail_db, qbuf))
			display_mysql_error();
		free(qbuf);
	}

	free(mbuf);

	dw_mutex_unlock(backend_mtx);
	return 0;
}

/* This function is here because the Visual C CRT doesn't
 * like free()ing memory allocated in other modules.
 */
void API backend_free(Account *acc, void *buf)
{
	if(buf)
		free(buf);
}

