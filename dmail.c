/* $Id: dmail.c,v 1.20 2005/06/08 15:53:52 crazybonz Exp $ */

#include "dwcompat.h"
#include "dw.h"
#include "backend.h"
#include "dmail.h"
#include "site.h"
#include "dmailextr.h"
#include "sendrecv.h"
#include "parse.h"

HWND hwndFrame, hwndTree, hwndContainer;
HMENUI menubar;

extern BackendPlugin plugin_list[];
AccountInfo AccountList[ACCOUNT_MAX];
DmailConfig config;

HMTX mutex;
char empty_string[] = "";

HWND in_about = 0, hwndNBK;
HWND stext1, stext2, stext3;

HICN fileicon, foldericon, linkicon;

/* Used for popup menus */
char *contexttext = NULL;
int message_dialog(MailParsed *mp, int readonly, int reply, char *text);
int modal_account_dialog(AccountSettings *as);
int currentpage=-1, pagescreated = 0, newpage = -1, firstpage = TRUE;
void DWSIGNAL account_dialog(HWND handle, void *data);

typedef struct _ip {
#ifdef _BIG_ENDIAN
	unsigned char ip4, ip3, ip2, ip1;
#else
	unsigned char ip1, ip2, ip3, ip4;
#endif
} IP4;

union ip4_32 {
	IP4 ip4;
	unsigned long ip32;
};

/* Parses out IP information from a string returned from the server */
void getipport(char *buffer, union ip4_32 *ip, union ip4_32 *port)
{
	char *buf2;
	int z, pos=0, num=0, len = strlen(buffer);

	buf2 = malloc(len+1);
	strcpy(buf2, buffer);

	port->ip32 = 0;
	ip->ip32 = 0;

	for(z=0;z<len;z++)
	{
		if(buf2[z] == '(')
			pos=z+1;
		if(buf2[z] == ',' || buffer[z] == ')')
		{
			unsigned char blah;
			buf2[z] = 0;
			blah = (unsigned char)atoi(&buf2[pos]);
			pos=z+1;
			switch(num)
			{
			case 0:
				ip->ip4.ip4 = blah;
				break;
			case 1:
				ip->ip4.ip3 = blah;
				break;
			case 2:
				ip->ip4.ip2 = blah;
				break;
			case 3:
				ip->ip4.ip1 = blah;
				break;
			case 4:
				port->ip4.ip2 = blah;
				break;
			case 5:
				port->ip4.ip1 = blah;
				break;
			}
			num++;
		}
	}
	free(buf2);
}

/* Generic function to parse information from a config file */
void dmail_getline(FILE *f, char *entry, char *entrydata)
{
	char in[256];
	int z;

	memset(in, 0, 256);
	fgets(in, 255, f);

	if(in[strlen(in)-1] == '\n')
		in[strlen(in)-1] = 0;

	if(in[0] != '#')
	{
		for(z=0;z<strlen(in);z++)
		{
			if(in[z] == '=')
			{
				in[z] = 0;
				strcpy(entry, in);
				strcpy(entrydata, &in[z+1]);
				return;
			}
		}
	}
	strcpy(entry, "");
	strcpy(entrydata, "");
}

/* Returns the AccountInfo pointer from an opaque type
 * or it returns NULL on failure.
 */
AccountInfo *findaccount(void *opaque)
{
	int n;
	AccountInfo *ai = opaque;

	if(ai &&
	   (ai->Type == DATA_TYPE_FOLDER ||
	   ai->Type == DATA_TYPE_ITEM ||
	   ai->Type == DATA_TYPE_ACCOUNT))
	{
		void *Acc;

		if(ai->Type == DATA_TYPE_ACCOUNT)
			Acc = ai;
		else
			Acc = ai->Acc;

		for(n=0;n<ACCOUNT_MAX;n++)
		{
			if(AccountList[n].Acc == Acc)
				return &AccountList[n];
		}
	}
	return NULL;
}

AccountSettings *findsettings(void *opaque)
{
	int n;
	AccountInfo *ai = opaque;

	if(ai &&
	   (ai->Type == DATA_TYPE_FOLDER ||
	   ai->Type == DATA_TYPE_ITEM ||
	   ai->Type == DATA_TYPE_ACCOUNT))
	{
		void *Acc;

		if(ai->Type == DATA_TYPE_ACCOUNT)
			Acc = ai;
		else
			Acc = ai->Acc;

		for(n=0;n<ACCOUNT_MAX;n++)
		{
			if(AccountList[n].Acc == Acc)
				return &AccountList[n].Settings;
		}
	}
	return NULL;
}

/* Find an account mailbox from it's name */
MailFolder *findfolder(AccountInfo *ai, char *name)
{
	int n = 0;

	while(ai && ai->Folders[n].Name[0])
	{
		if(strcmp(ai->Folders[n].Name, name) == 0)
			return &ai->Folders[n];
		n++;
	}
	return NULL;
}

/* Move item from one folder to the next */
void moveitem(AccountInfo *ai, MailItem *mi, MailFolder *mf)
{
	MailItem mailitem;
	char *data;
	unsigned long datalen;

	data = plugin_list[ai->Plug].getmail(mi->Acc, mi->Folder, mi, &datalen);
	memcpy(&mailitem, mi, sizeof(MailItem));
	plugin_list[ai->Plug].newitem(ai->Acc, mf, &mailitem);
	plugin_list[ai->Plug].newmail(ai->Acc, mf, &mailitem, data, datalen);
	plugin_list[ai->Plug].free(mi->Acc, data);
	plugin_list[ai->Plug].delitem(mi->Acc, mi->Folder, mi);
}

/* Make a DOS style path into something a web browser can handle. */
void urlify(char *buf)
{
	int z, len = strlen(buf);

	for(z=0;z<len;z++)
	{
		if(buf[z] == '\\')
			buf[z] = '/';
		if(buf[z] == ':')
			buf[z] = '|';
	}
}

void dmail_init_tree(void)
{
	HTREEITEM maint, tmp;
    int z;

	for(z=0;z<PLUGIN_MAX;z++)
	{
		if(plugin_list[z].hMod)
		{
			char **accs = plugin_list[z].getaccounts();
			int x;

			for(x=0;x<ACCOUNT_MAX;x++)
			{
				if(!AccountList[x].Acc)
					break;
			}

			/* Initialize our account */
			AccountList[x].Plug = z;
			AccountList[x].SendEve = dw_event_new();
			AccountList[x].RecvEve = dw_event_new();
			dw_event_reset(AccountList[x].SendEve);
			dw_event_reset(AccountList[x].RecvEve);

			if(!accs)
			{
				AccountSettings *set = &AccountList[x].Settings;

				if(!modal_account_dialog(set))
				{
					/* Initialize the deafult settings */
					strcpy(set->AccountName, "Default");
					set->RecvHostType = HOST_TYPE_POP3;
					strcpy(set->RecvHostName, "pop3.server.com");
					set->RecvHostPort = 110;
					strcpy(set->RecvHostUser, "username");
					strcpy(set->RecvHostPass, "password");
					set->SendHostType = HOST_TYPE_SMTP;
					strcpy(set->SendHostName, "smtp.server.com");
					set->SendHostPort = 25;
					strcpy(set->SendHostUser, "");
					strcpy(set->SendHostPass, "");
				}
				AccountList[x].Acc = plugin_list[z].newaccount(set->AccountName);
				AccountList[x].Folders = plugin_list[z].getfolders(AccountList[x].Acc);


				plugin_list[z].newsettings(AccountList[x].Acc, set);

				/* Populate our tree */
				maint = dw_tree_insert(hwndTree, set->AccountName, fileicon, 0, 0);
				dw_tree_item_set_data(hwndTree, maint, &AccountList[x]);

				if(AccountList[x].Folders)
				{
					int y = 0;

					while(AccountList[x].Folders[y].Name[0])
					{
						tmp = dw_tree_insert(hwndTree, AccountList[x].Folders[y].Name, foldericon, maint, 0);
						dw_tree_item_set_data(hwndTree, tmp, &AccountList[x].Folders[y]);
						y++;
					}
				}
				dw_tree_item_expand(hwndTree, maint);
			}
			else
			{
				int x = 0;

				while(accs[x])
				{
					AccountList[x].Acc = plugin_list[z].openaccount(accs[x]);
					AccountList[x].Folders = plugin_list[z].getfolders(AccountList[x].Acc);
					AccountList[x].Plug = z;

					/* Load our settings */
					plugin_list[z].getsettings(AccountList[x].Acc, &AccountList[x].Settings);

					/* Populate our tree */
					maint = dw_tree_insert(hwndTree, accs[x], fileicon, 0, 0);
					dw_tree_item_set_data(hwndTree, maint, AccountList[x].Acc);

					if(AccountList[x].Folders)
					{
						int y = 0;

						while(AccountList[x].Folders[y].Name[0])
						{
							tmp = dw_tree_insert(hwndTree, AccountList[x].Folders[y].Name, foldericon, maint, 0);
							dw_tree_item_set_data(hwndTree, tmp, &AccountList[x].Folders[y]);
							y++;
						}
					}
					dw_tree_item_expand(hwndTree, maint);

					x++;
				}
			}

			/* Start the send and receive threads */
			dw_thread_new((void *)Mail_Send_Thread, (void *)&AccountList[x], 20000);
			dw_thread_new((void *)Mail_Receive_Thread, (void *)&AccountList[x], 20000);
		}
	}
}

void setfoldercount(MailFolder *mf, int count)
{
	char textbuf[1024] = "Welcome to Dynamic Mail";
	if(mf && mf->Type == DATA_TYPE_FOLDER)
		sprintf(textbuf, "%s (%d message%s)", mf->Name, count, count == 1 ? "" : "s");
	else
		count = 0;
	dw_window_set_text(stext1, textbuf);
	dw_window_set_data(stext1, "count", DW_INT_TO_POINTER(count));
}

int DWSIGNAL generic_cancel(HWND window, void *data)
{
        UserEntry *param = (UserEntry *)data;

        if(param)
        {
                dw_window_destroy(param->window);
                if(param->busy)
                        *param->busy = 0;
                if(param->data)
                        free(param->data);
                free(param);
        }
        return FALSE;
}


/* Open a mail viewer window when double clicking on a mail item */
int DWSIGNAL deleteitem(HWND hwnd, void *data)
{
	MailItem *mi = (MailItem *)dw_container_query_start(hwndContainer, DW_CRA_SELECTED);
	int plug = DW_POINTER_TO_INT(dw_window_get_data(hwndContainer, "plug"));
	AccountInfo *ai = findaccount(mi);
	MailFolder *mf = findfolder(ai, "Trash");

	if(dw_messagebox("DynamicMail", DW_MB_YESNO | DW_MB_QUESTION, "Are you sure you want to delete the selected messages?"))
	{
		int deleted = 0, count = DW_POINTER_TO_INT(dw_window_get_data(stext1, "count"));

		while(mi)
		{
			MailItem *lastitem = NULL;

			if(mi->Type == DATA_TYPE_ITEM)
			{

				/* If we have a trash folder, move the message there
				 * otherwise delete the message immediately.
				 */
				if(mf && mf != mi->Folder)
					moveitem(ai, mi, mf);
				else
					plugin_list[plug].delitem(mi->Acc, mi->Folder, mi);

				lastitem = mi;

				deleted++;
			}
			mi = (MailItem *)dw_container_query_next(hwndContainer, DW_CRA_SELECTED);

			/* Delete the item after querying the next, as the next
			 * item is dependant on the last item.
			 */
			if(lastitem)
				dw_container_delete_row(hwndContainer, (char *)lastitem);
		}
		setfoldercount(mf, count - deleted);
	}
	return TRUE;
}

/* Callback to handle right clicks on the container */
int DWSIGNAL containercontextmenu(HWND hwnd, char *text, int x, int y, void *data)
{
	if(text)
	{
		HMENUI hwndMenu, submenu;
		HWND menuitem;

		hwndMenu = dw_menu_new(0L);

		menuitem = dw_menu_append_item(hwndMenu, locale_string("New Message", 20), IDM_NEWMESS, 0L, TRUE, FALSE, DW_NOMENU);
		dw_signal_connect(menuitem, DW_SIGNAL_CLICKED, DW_SIGNAL_FUNC(new_message), NULL);

		menuitem = dw_menu_append_item(hwndMenu, "", 0L, 0L, TRUE, FALSE, DW_NOMENU);

		submenu = dw_menu_new(0L);

		menuitem = dw_menu_append_item(submenu, locale_string("Including Quoting", 20), IDM_REPLYQ, 0L, TRUE, FALSE, DW_NOMENU);
		dw_signal_connect(menuitem, DW_SIGNAL_CLICKED, DW_SIGNAL_FUNC(replyq_message), (void *)text);
		menuitem = dw_menu_append_item(submenu, locale_string("Excluding Quoting", 20), IDM_REPLY, 0L, TRUE, FALSE, DW_NOMENU);
		dw_signal_connect(menuitem, DW_SIGNAL_CLICKED, DW_SIGNAL_FUNC(reply_message), (void *)text);

		menuitem = dw_menu_append_item(hwndMenu, locale_string("Reply To", 20), IDM_REPLY, 0L, TRUE, FALSE, submenu);

		submenu = dw_menu_new(0L);

		menuitem = dw_menu_append_item(submenu, locale_string("Including Quoting", 20), IDM_REPLYALLQ, 0L, TRUE, FALSE, DW_NOMENU);
		dw_signal_connect(menuitem, DW_SIGNAL_CLICKED, DW_SIGNAL_FUNC(replyallq_message), (void *)text);
		menuitem = dw_menu_append_item(submenu, locale_string("Excluding Quoting", 20), IDM_REPLYALL, 0L, TRUE, FALSE, DW_NOMENU);
		dw_signal_connect(menuitem, DW_SIGNAL_CLICKED, DW_SIGNAL_FUNC(replyall_message), (void *)text);

		menuitem = dw_menu_append_item(hwndMenu, locale_string("Reply To All", 20), IDM_REPLY, 0L, TRUE, FALSE, submenu);

		menuitem = dw_menu_append_item(hwndMenu, "", 0L, 0L, TRUE, FALSE, DW_NOMENU);

		menuitem = dw_menu_append_item(hwndMenu, locale_string("Delete", 21), IDM_DELITEM, 0L, TRUE, FALSE, DW_NOMENU);
		dw_signal_connect(menuitem, DW_SIGNAL_CLICKED, DW_SIGNAL_FUNC(deleteitem), (void *)text);
		dw_menu_popup(&hwndMenu, hwndFrame, x, y);
	}

	return FALSE;  
}

/* Callback to handle right clicks on the container */
static int DWSIGNAL treecontextmenu(HWND hwnd, char *text, int x, int y, void *data, void *itemdata)
{
	if(text)
	{
		HMENUI hwndMenu;
		HWND menuitem;

		hwndMenu = dw_menu_new(0L);

		menuitem = dw_menu_append_item(hwndMenu, locale_string("New Folder", 20), IDM_NEWFOLD, 0L, TRUE, FALSE, DW_NOMENU);
		dw_signal_connect(menuitem, DW_SIGNAL_CLICKED, DW_SIGNAL_FUNC(new_folder), itemdata);

		menuitem = dw_menu_append_item(hwndMenu, "", 0L, 0L, TRUE, FALSE, DW_NOMENU);

		menuitem = dw_menu_append_item(hwndMenu, locale_string("Account Settings", 20), IDM_ACCOUNT, 0L, TRUE, FALSE, DW_NOMENU);
		dw_signal_connect(menuitem, DW_SIGNAL_CLICKED, DW_SIGNAL_FUNC(account_dialog), itemdata);

		if(strcasecmp(text, "Trash") == 0)
		{
			menuitem = dw_menu_append_item(hwndMenu, "", 0L, 0L, TRUE, FALSE, DW_NOMENU);

			menuitem = dw_menu_append_item(hwndMenu, locale_string("Empty Trash", 20), IDM_TRASH, 0L, TRUE, FALSE, DW_NOMENU);
			dw_signal_connect(menuitem, DW_SIGNAL_CLICKED, DW_SIGNAL_FUNC(empty_trash), itemdata);
		}
		dw_menu_popup(&hwndMenu, hwndFrame, x, y);
	}
	return TRUE;
}

/* Open a mail viewer window when double clicking on a mail item */
int DWSIGNAL containerselect(HWND hwnd, char *text, void *data)
{
	MailItem *mi = (MailItem *)text;
	int plug = DW_POINTER_TO_INT(dw_window_get_data(hwndContainer, "plug"));

	if(mi && mi->Type == DATA_TYPE_ITEM)
	{
		unsigned long len;
		char *mytext = plugin_list[plug].getmail(mi->Acc, mi->Folder, mi, &len);

		if(mytext)
		{
			MailParsed *mp = malloc(sizeof(MailParsed));

			char *view = parse_message(mytext, len, mp);

			message_dialog(mp, TRUE, FALSE, view);
			plugin_list[plug].free(mi->Acc, mytext);
		}
	}
	return TRUE;
}

/* Display the mail in the preview window */
int DWSIGNAL containerfocus(HWND hwnd, HWND treeitem, char *text, void *data)
{
	MailItem *mi = (MailItem *)text;
	int plug = DW_POINTER_TO_INT(dw_window_get_data(hwndContainer, "plug"));

	if(mi && mi->Type == DATA_TYPE_ITEM)
	{
		unsigned long len;
		char *mytext = plugin_list[plug].getmail(mi->Acc, mi->Folder, mi, &len);
		HWND mle = (HWND)dw_window_get_data(hwnd, "mle");

		if(mytext)
		{
			if(mle)
			{
				char *view = parse_message(mytext, len, NULL);

				dw_mle_clear(mle);
				dw_mle_import(mle, view, -1);
				dw_mle_set_cursor(mle, 0);
			}
			plugin_list[plug].free(mi->Acc, mytext);
		}
	}
	return TRUE;
}

/* Update the message list when a user selects a tree item */
int DWSIGNAL tree_select(HWND handle, HTREEITEM item, char *text, void *date, void *itemdata)
{
	void *cdata = dw_window_get_data(hwndContainer, "items");
	MailFolder *mf = (MailFolder *)itemdata;
	int clear = TRUE;
	HWND mle = (HWND)dw_window_get_data(hwndContainer, "mle");
	int plug = DW_POINTER_TO_INT(dw_window_get_data(hwndContainer, "plug"));
	MailItem *olditems = (MailItem *)cdata;
	int count = 0;

	/* Don't redraw if we don't have to */
	if(olditems && olditems->Type == DATA_TYPE_ITEM && olditems->Folder == mf)
		return TRUE;

	dw_window_set_data(hwndContainer, "items", NULL);

	if(mle)
		dw_mle_clear(mle);

	/* If the user clicked on a tree folder, populate the container
	 * with the contents of that folder.  Otherwise clear the container.
	 */
	if(mf && mf->Type == DATA_TYPE_FOLDER)
	{
		MailItem *mi;
		AccountInfo *ai = findaccount(mf);

		if(ai)
		{
			plug = ai->Plug;
			dw_window_set_data(hwndContainer, "plug", DW_INT_TO_POINTER(plug));
			dw_window_set_data(hwndContainer, "account", (void *)ai);

			mi = plugin_list[plug].getitems(mf->Acc, mf);

			dw_window_set_data(hwndContainer, "items", mi);

			if(mi)
			{
				void *containerinfo;
				int z;

				while(mi[count].Id)
					count++;

				if(count)
				{
					dw_container_clear(hwndContainer, FALSE);

					containerinfo = dw_container_alloc(hwndContainer, count);

					for(z=0;z<count;z++)
					{
						unsigned long size = mi[z].Size;
						char *Topic = mi[z].Topic;
						char *From = mi[z].From;

						/* Icons */
						dw_container_set_item(hwndContainer, containerinfo, 0, z, &fileicon);
						dw_container_set_item(hwndContainer, containerinfo, 1, z, &foldericon);
						dw_container_set_item(hwndContainer, containerinfo, 2, z, &Topic);
						dw_container_set_item(hwndContainer, containerinfo, 3, z, &From);
						/* Date and Time */
						dw_container_set_item(hwndContainer, containerinfo, 4, z, &(mi[z].Date));
						dw_container_set_item(hwndContainer, containerinfo, 5, z, &(mi[z].Time));
						dw_container_set_item(hwndContainer, containerinfo, 6, z, &size);
						dw_container_set_row_title(containerinfo, z, (char *)&(mi[z]));
					}

					dw_container_insert(hwndContainer, containerinfo, count);
					dw_container_optimize(hwndContainer);
					clear = FALSE;
				}
			}
		}

		if(clear)
			dw_container_clear(hwndContainer, TRUE);
	}
	else
		dw_container_clear(hwndContainer, TRUE);
	if(mf && cdata)
		plugin_list[plug].free(mf->Acc, cdata);
	setfoldercount(mf, count);
	return TRUE;
}

int DWSIGNAL exitfunc(HWND hwnd, void *data)
{
	if(dw_messagebox("DynamicMail", DW_MB_YESNO | DW_MB_QUESTION, "Are you sure you want to exit DynamicMail?"))
	{
		dw_main_quit();
	}
	return TRUE;
}

/* Delete event */
int DWSIGNAL message_deleteevent(HWND hwnd, void *data)
{
	int readonly = DW_POINTER_TO_INT(dw_window_get_data(hwnd, "readonly"));

	if(readonly || (!readonly && dw_messagebox("DynamicMail", DW_MB_YESNO | DW_MB_QUESTION, "Are you sure you want to close this unsent message?")))
	{
		MailParsed *mp = (MailParsed *)dw_window_get_data(hwnd, "MailParsed");

		if(mp)
		{
			parse_free(mp);
			free(mp);
		}
		dw_window_destroy(hwnd);
	}
	return TRUE;
}

/* Handle user request to show a raw message */
int DWSIGNAL show_raw(HWND hwnd, void *data)
{
	HWND window = (HWND)data;
	HWND mle = (HWND)dw_window_get_data(window, "mle");
	MailParsed *mp = (MailParsed *)dw_window_get_data(window, "MailParsed");

	if(mp)
	{
		dw_mle_clear(mle);
		if(dw_window_get_data(window, "raw"))
		{
			dw_window_set_data(window, "raw", 0);
			if(mp->text[0])
				dw_mle_import(mle, mp->text[0], -1);
			else if(mp->html[0])
				dw_mle_import(mle, mp->html[0], -1);
		}
		else
		{
			dw_window_set_data(window, "raw", (void *)1);
			if(mp->raw)
				dw_mle_import(mle, mp->raw, -1);
		}
		dw_mle_set_cursor(mle, 0);
	}
	return 0;
}

/* Handle user request to save the main window position */
int DWSIGNAL save_position(HWND hwnd, void *data)
{
	float pos;
	HWND hsplit = (HWND)dw_window_get_data(hwndFrame, "hsplit");
	HWND vsplit = (HWND)dw_window_get_data(hwndFrame, "vsplit");

	dw_window_get_pos_size(hwndFrame, &config.x, &config.y, &config.width, &config.height);
	if(hsplit)
	{
		pos = dw_splitbar_get(hsplit);
		config.hsplit = (unsigned long)pos;
	}
	if(vsplit)
	{
		pos = dw_splitbar_get(vsplit);
		config.vsplit = (unsigned long)pos;
	}
	saveconfig();
	return TRUE;
}

/* Handle user request to send newly composed message */
int DWSIGNAL send_message(HWND hwnd, void *data)
{
	HWND window = (HWND)data;
	HWND mle = (HWND)dw_window_get_data(window, "mle");
	HWND to = (HWND)dw_window_get_data(window, "entry1");
	HWND from = (HWND)dw_window_get_data(window, "entry2");
	HWND subject = (HWND)dw_window_get_data(window, "entry3");
	HWND listbox1 = (HWND)dw_window_get_data(window, "listbox1");
	HWND listbox2 = (HWND)dw_window_get_data(window, "listbox2");
	int lcount, len, n;
	AccountInfo *ai = (AccountInfo *)dw_window_get_data(hwndContainer, "account");
	MailItem mi;
	MailFolder *mf = NULL;
	char *text, tmpbuf[41];
	DWEnv env;
	time_t now;
	struct tm time_val;

	if(!ai)
		ai = &AccountList[0];

	if(dw_window_get_data(window, "readonly"))
		return TRUE;

	memset(&mi, 0, sizeof(MailItem));

	text = dw_window_get_text(to);

	/* Query user entered text */
	if(text)
	{
		if(*text)
			strcpy(mi.To, text);
		dw_free(text);
	}

	text = dw_window_get_text(subject);
	if(text)
	{
		strcpy(mi.Topic, text);
		dw_free(text);
	}

	/* User the email from the account settings for sending */
	sprintf(mi.From, "%s <%s>", ai->Settings.UserRealName,
			ai->Settings.UserEmail);

	lcount = dw_listbox_count(listbox1);

	if(!mi.To[0] && lcount)
		dw_listbox_get_text(listbox1, 0, mi.To, MAIL_ITEM_MAX);

	/* FIXME: This should query the text length! */
	text = calloc(1, 200000);

	/* Create a header */
	dw_environment_query(&env);
	now = time(NULL);
	time_val = *localtime(&now);
	strftime(tmpbuf, 40, "%a, %d %b %Y %H:%M:%S", &time_val);
	strcat(text, tmpbuf);

	sprintf(text, "Date: %s\r\nFrom: %s\r\nUser-Agent: DynamicMail 0.1 (%s %d.%d)\r\nMIME-Version: 1.0\r\nTo: ",
			tmpbuf, mi.From, env.osName, env.MajorVersion, env.MinorVersion);

	strcat(text, mi.To);

	/* Create the to line */
	for(n=0;n<lcount;n++)
	{
		char To[MAIL_ITEM_MAX+1];

		dw_listbox_get_text(listbox1, n, To, MAIL_ITEM_MAX);
		if(To[0] && (n || (!n && strncmp(mi.To, To, MAIL_ITEM_MAX))))
		{
			strcat(text, ",\r\n ");
			strcat(text, To);
		}
	}

	/* If the Reply-to information is set, use it */
	if(ai->Settings.ReplyRealName[0] && ai->Settings.ReplyEmail[0])
	{
		strcat(text, "\r\nReply-to: ");
		strcat(text, ai->Settings.ReplyRealName);
		strcat(text, " <");
		strcat(text, ai->Settings.ReplyEmail);
		strcat(text, ">");
	}
	/* Create the subject and content lines */
	strcat(text, "\r\nSubject: ");
	strcat(text, mi.Topic);
	strcat(text, "\r\nContent-Type: text/plain\r\nContent-Transfer-Encoding: 7bit\r\n\r\n");

	n=0;

	len = strlen(text);
	dw_mle_export(mle, &text[len], 0, 200000 - len);

	/* Find the outbox */
	while(ai->Folders[n].Name[0])
	{
		if(strcmp(ai->Folders[n].Name, "Outbox") == 0)
		{
			mf = &ai->Folders[n];
			break;
		}
		n++;
	}

	/* Save the new message to the outbox */
	if(mf)
	{
		mi.Size = strlen(text);
		init_date_time(&mi.Date, &mi.Time);
		plugin_list[ai->Plug].newitem(ai->Acc, mf, &mi);
		plugin_list[ai->Plug].newmail(ai->Acc, mf, &mi, text, mi.Size);
	}

	/* Cleanup */
	free(text);

	/* Tell the send thread to go! */
	dw_event_post(ai->SendEve);

	/* Destroy the compose window */
	dw_window_destroy(window);
	return TRUE;
}

/* Handle user request to send messages waiting in the outbox */
int DWSIGNAL send_mail(HWND hwnd, void *data)
{
	AccountInfo *ai = (AccountInfo *)dw_window_get_data(hwndContainer, "account");

	/* Tell the send thread to go! */
	dw_event_post(ai->SendEve);

	return TRUE;
}

/* Handle user request to check mail */
int DWSIGNAL check_mail(HWND hwnd, void *data)
{
	int z;

	for(z=0;z<ACCOUNT_MAX;z++)
	{
		if(AccountList[z].Acc)
			dw_event_post(AccountList[z].RecvEve);

	}
	return TRUE;
}

ULONG messageItems[]=
{
	NEWTAB      , REMOVETAB   , TB_SEPARATOR, CONNECT     , DISCONNECT  ,
	TB_SEPARATOR, IDM_EXIT    , TB_SEPARATOR, REMOVEFROMQ , ADDTOQ      ,
	FLUSHQ      , TB_SEPARATOR, SAVETITLE   , UNSAVETITLE , TB_SEPARATOR,
	PB_CHANGE   , TB_SEPARATOR, IDM_PREFERENCES , ADMIN       , TB_SEPARATOR,
	IDM_GENERALHELP, IDM_ABOUT   , 0
};

char messageHelpItems[][100]=
{
	"Send Message", "Remove Tab", "", "Connect", "Disconnect",
	"", "Exit", "", "Remove From Queue", "Add to Queue",
	"Flush Queue", "", "Save Title", "Unsave Title", "",
	"Refresh", "", "Preferences", "Toggle Raw Message", "",
	"General Help", "About"
};

void *messageFunctions[] = { (void *)send_message, NULL, NULL, NULL, NULL,
						  NULL, NULL, NULL, NULL, NULL,
						  NULL, NULL, NULL, NULL, NULL,
						  NULL, NULL, NULL, NULL, (void *)show_raw,
						  NULL, (void *)abouttab };


/* Create the about box */
void about(void)
{
	HWND infowindow, mainbox, logo, okbutton, buttonbox, stext, mle;
	UserEntry *param = malloc(sizeof(UserEntry));
	ULONG flStyle = DW_FCF_TITLEBAR | DW_FCF_SHELLPOSITION | DW_FCF_DLGBORDER;
	ULONG point = -1;
	char *greets = "Thanks to the OS/2 Netlabs, OS2.org, " \
                      "\r\nAnd of course Brian for starting it. ;)";

	if(in_about)
	{
		dw_window_show(in_about);
		return;
	}

	in_about = infowindow = dw_window_new(HWND_DESKTOP, locale_string("About DMail", 54), flStyle);

	mainbox = dw_box_new(BOXVERT, 5);

	dw_box_pack_start(infowindow, mainbox, 0, 0, TRUE, TRUE, 0);

	buttonbox = dw_box_new(BOXHORZ, 0);

	dw_box_pack_start(mainbox, buttonbox, 0, 0, TRUE, FALSE, 0);

	logo = dw_bitmap_new(100);

	dw_window_set_bitmap(logo, LOGO, NULL);

	dw_box_pack_start(buttonbox, 0, 50, 30, TRUE, FALSE, 0);
	dw_box_pack_start(buttonbox, logo, 324, 172, FALSE, FALSE, 2);
	dw_box_pack_start(buttonbox, 0, 50, 30, TRUE, FALSE, 0);

	stext = dw_text_new("DynamicMail (c) 2000-2011 Brian Smith", 0);
	dw_window_set_style(stext, DW_DT_CENTER | DW_DT_VCENTER, DW_DT_CENTER | DW_DT_VCENTER);
	dw_box_pack_start(mainbox, stext, 10, 20, TRUE, TRUE, 0);

	mle = dw_mle_new(100L);

	dw_box_pack_start(mainbox, mle, 130, 150, TRUE, TRUE, 2);

	dw_mle_set_editable(mle, FALSE);
	dw_mle_set_word_wrap(mle, TRUE);

	/* Buttons */
	buttonbox = dw_box_new(BOXHORZ, 0);

	dw_box_pack_start(mainbox, buttonbox, 0, 0, TRUE, TRUE, 0);

	okbutton = dw_button_new("Ok", 1001L);

	dw_box_pack_start(buttonbox, 0, 50, 30, TRUE, FALSE, 0);
	dw_box_pack_start(buttonbox, okbutton, 50, 40, FALSE, FALSE, 2);
	dw_box_pack_start(buttonbox, 0, 50, 30, TRUE, FALSE, 0);

	param->window = infowindow;
	param->data = NULL;
	param->busy = &in_about;

	point = dw_mle_import(mle, greets, point);

	dw_signal_connect(okbutton, DW_SIGNAL_CLICKED, DW_SIGNAL_FUNC(generic_cancel), (void *)param);

	dw_window_set_size(infowindow, 460, 441);

	dw_window_show(infowindow);
}

/* IDM_ABOUT */
int DWSIGNAL abouttab(HWND hwnd, void *data)
{
/*        if(validatecurrentpage())  */
                about();
        return FALSE;
}

/* Generic multi-use function for creating mail reading and composing windows */
int message_dialog(MailParsed *mp, int readonly, int reply, char *text)
{
	HWND window, mainbox, toolbox, hbox, stext, entry, mle, button, listbox, controlbox, tempbutton, menuitem;
	HMENUI menu;
	ULONG flStyle = DW_FCF_SYSMENU | DW_FCF_TITLEBAR | DW_FCF_SIZEBORDER | DW_FCF_MINMAX |
		DW_FCF_SHELLPOSITION | DW_FCF_TASKLIST | DW_FCF_DLGBORDER;
	int z = 0;

	window = dw_window_new(HWND_DESKTOP, mp ? (mp->topic ? mp->topic : "") : "New Message", flStyle);

	dw_window_set_icon(window, DW_RESOURCE(MAIN_FRAME));

	if(readonly)
		dw_window_set_data(window, "readonly", (void *)1);

	if(mp)
		dw_window_set_data(window, "MailParsed", (void *)mp);

	mainbox = dw_box_new(BOXVERT, 0);

	dw_box_pack_start(window, mainbox, 0, 0, TRUE, TRUE, 0);

	toolbox = dw_box_new(BOXHORZ, 1);

	dw_box_pack_start(mainbox, toolbox, 0, 0, TRUE, FALSE, 0);

	/* Create the toolbar */
	while(messageItems[z])
	{
		if(messageItems[z] != -1)
		{
			tempbutton = dw_bitmapbutton_new(messageHelpItems[z], messageItems[z]);
            dw_window_set_style(tempbutton, DW_BS_NOBORDER, DW_BS_NOBORDER);

			if(messageFunctions[z])
				dw_signal_connect(tempbutton, DW_SIGNAL_CLICKED, DW_SIGNAL_FUNC(messageFunctions[z]), (void *)window);

			dw_box_pack_start(toolbox, tempbutton, 30, 30, FALSE, FALSE, 0);
		}
		else
			dw_box_pack_start(toolbox, 0, 5, 30, FALSE, FALSE, 0);
		z++;
	}
	dw_box_pack_start(toolbox, 0, 3, 30, TRUE, FALSE, 0);

	hbox = dw_box_new(BOXHORZ, 0);

	dw_box_pack_start(mainbox, hbox, 0, 0, TRUE, FALSE, 0);

    if(readonly)
		stext = dw_text_new("From:", 0);
	else
		stext = dw_text_new("To:", 0);
	dw_window_set_style(stext, DW_DT_VCENTER | DW_DT_CENTER, DW_DT_VCENTER | DW_DT_CENTER);

	dw_box_pack_start(hbox, stext, 40, 22, FALSE, FALSE, 0);

	entry = dw_entryfield_new("", 0);
	dw_entryfield_set_limit(entry, MAIL_ITEM_MAX);

	dw_window_set_data(window, "entry1", (void *)entry);

	if(mp && readonly)
		dw_window_set_text(entry, mp->from);

	if(readonly)
		dw_window_disable(entry);

	dw_box_pack_start(hbox, entry, 100, 22, TRUE, FALSE, 0);

	if(readonly)
	{
		stext = dw_text_new("Date:", 0);
		dw_window_set_style(stext, DW_DT_VCENTER | DW_DT_CENTER, DW_DT_VCENTER | DW_DT_CENTER);

		dw_box_pack_start(hbox, stext, 40, 22, FALSE, FALSE, 0);
	}
	else
	{
		button = dw_button_new("Cc:", 0);

		dw_box_pack_start(hbox, button, 40, 22, FALSE, FALSE, 0);
	}

	entry = dw_entryfield_new("", 0);
	dw_entryfield_set_limit(entry, MAIL_ITEM_MAX);

	dw_window_set_data(window, "entry2", (void *)entry);

	if(readonly)
	{
		dw_window_disable(entry);

		if(mp && mp->date)
			dw_window_set_text(entry, mp->date);
	}

	dw_box_pack_start(hbox, entry, 100, 22, TRUE, FALSE, 0);

	hbox = dw_box_new(BOXHORZ, 0);

	dw_box_pack_start(mainbox, hbox, 0, 0, TRUE, FALSE, 0);

	if(readonly)
	{
		stext = dw_text_new("To:", 0);
		dw_window_set_style(stext, DW_DT_TOP | DW_DT_CENTER, DW_DT_TOP | DW_DT_CENTER);

		dw_box_pack_start(hbox, stext, 40, 44, FALSE, FALSE, 0);
	}

	listbox = dw_listbox_new(0, FALSE);

	dw_window_set_data(window, "listbox1", (void *)listbox);

	if(readonly || reply)
	{
		if(mp)
		{
			/* Fill in all the to addresses into the listbox */
			int n = 0;

			if(reply && mp)
			{
				if(mp->replyto)
					dw_listbox_append(listbox, mp->replyto);
				else
					dw_listbox_append(listbox, mp->from);
			}

			if(reply == IDM_REPLYALL || reply == IDM_REPLYALLQ || !reply)
			{
				while(mp->to[n] && n < MAX_TO_ADDRESSES)
				{
					dw_listbox_append(listbox, mp->to[n]);
					n++;
				}
			}
		}
		if(readonly)
			dw_window_disable(listbox);
	}

	dw_box_pack_start(hbox, listbox, 100, 44, TRUE, FALSE, 0);

	if(readonly)
	{
		stext = dw_text_new("Cc:", 0);
		dw_window_set_style(stext, DW_DT_TOP | DW_DT_CENTER, DW_DT_TOP | DW_DT_CENTER);

		dw_box_pack_start(hbox, stext, 40, 44, FALSE, FALSE, 0);
	}

	listbox = dw_listbox_new(0, FALSE);

	dw_window_set_data(window, "listbox2", (void *)listbox);

	if(readonly)
		dw_window_disable(listbox);

	dw_box_pack_start(hbox, listbox, 100, 44, TRUE, FALSE, 0);

	hbox = dw_box_new(BOXHORZ, 0);

	dw_box_pack_start(mainbox, hbox, 0, 0, TRUE, FALSE, 0);

	stext = dw_text_new("Subject:", 0);
	dw_window_set_style(stext, DW_DT_VCENTER | DW_DT_CENTER, DW_DT_VCENTER | DW_DT_CENTER);

	dw_box_pack_start(hbox, stext, 50, 22, FALSE, FALSE, 0);

	entry = dw_entryfield_new("", 0);
	dw_entryfield_set_limit(entry, MAIL_ITEM_MAX);

	dw_window_set_data(window, "entry3", (void *)entry);

	if(mp)
	{
		if(reply && strncasecmp(mp->topic, "Re:", 3))
		{
			char *tmp = malloc(strlen(mp->topic) + 5);

			sprintf(tmp, "Re: %s", mp->topic);
			dw_window_set_text(entry, tmp);
			free(tmp);
		}
		else
			dw_window_set_text(entry, mp->topic);
	}

	if(readonly)
		dw_window_disable(entry);

	dw_box_pack_start(hbox, entry, 100, 22, TRUE, FALSE, 0);

	mle = dw_mle_new(0);

	dw_window_set_data(window, "mle", (void *)mle);

	if(reply == IDM_REPLYQ || reply == IDM_REPLYALLQ)
	{
		char *oldtext = NULL;

		if(mp && mp->text[0])
			oldtext = mp->text[0];
		else if(mp && mp->html[0])
			oldtext = mp->html[0];
		else 
			oldtext = text;

		if(oldtext)
		{
			int lines = count_lines(oldtext);
			char *newtext = malloc(strlen(oldtext) + lines + 1);
			make_reply(oldtext, newtext);
			dw_mle_import(mle, newtext, -1);
			dw_mle_set_cursor(mle, 0);
			free(newtext);
		}
	}
	else if(!reply)
	{
		if(mp && mp->text[0])
			dw_mle_import(mle, mp->text[0], -1);
		else if(mp && mp->html[0])
			dw_mle_import(mle, mp->html[0], -1);
		else if(text)
			dw_mle_import(mle, text, -1);
		dw_mle_set_cursor(mle, 0);
	}

	dw_mle_set_editable(mle, readonly ? FALSE : TRUE);
	dw_mle_set_word_wrap(mle, TRUE);

	dw_box_pack_start(mainbox, mle, 100, 100, TRUE, TRUE, 0);

	controlbox = dw_box_new(BOXHORZ, 0);

	dw_box_pack_start(mainbox, controlbox, 0, 0, TRUE, FALSE, 0);

	stext = dw_status_text_new("Welcome to DynamicMail.", 1026);

	dw_window_set_style(stext, DW_DT_VCENTER, DW_DT_VCENTER);

	dw_box_pack_start(controlbox, stext, 100, -1, TRUE, FALSE, 2);

	stext = dw_status_text_new("Normal", 1026);

	dw_window_set_style(stext, DW_DT_VCENTER, DW_DT_VCENTER);

	dw_box_pack_start(controlbox, stext, 50, -1, FALSE, FALSE, 2);

	stext = dw_status_text_new("MIME", 1026);

	dw_window_set_style(stext, DW_DT_VCENTER, DW_DT_VCENTER);

	dw_box_pack_start(controlbox, stext, 50, -1, FALSE, FALSE, 2);

	menubar = dw_menubar_new(window);

	menu = dw_menu_new(0L);

	menuitem = dw_menu_append_item(menu, locale_string("Send", 20), IDM_SEND, 0L, TRUE, FALSE, DW_NOMENU);
	dw_signal_connect(menuitem, DW_SIGNAL_CLICKED, DW_SIGNAL_FUNC(send_message), (void *)window);

	menuitem = dw_menu_append_item(menu, locale_string("Save", 20), IDM_SAVE, 0L, TRUE, FALSE, DW_NOMENU);

	menuitem = dw_menu_append_item(menu, "", 0L, 0L, TRUE, FALSE, DW_NOMENU);

	menuitem = dw_menu_append_item(menu, locale_string("Toggle Raw", 20), IDM_RAWTOG, 0L, TRUE, FALSE, DW_NOMENU);
	dw_signal_connect(menuitem, DW_SIGNAL_CLICKED, DW_SIGNAL_FUNC(show_raw), (void *)window);

	menuitem = dw_menu_append_item(menubar, locale_string("~Message", 2), IDM_MESSAGE, 0L, TRUE, FALSE, menu);

	menu = dw_menu_new(0L);

	menuitem = dw_menu_append_item(menu, locale_string("~General Help", 20), IDM_GENERALHELP, 0L, TRUE, FALSE, DW_NOMENU);

	dw_menu_append_item(menu, "", 0L, 0L, TRUE, FALSE, DW_NOMENU);
	menuitem = dw_menu_append_item(menu, locale_string("~About", 23), IDM_ABOUT, 0L, TRUE, FALSE, DW_NOMENU);

	menuitem = dw_menu_append_item(menubar, locale_string("~Help", 2), IDM_HELP, 0L, TRUE, FALSE, menu);

	dw_signal_connect(window, DW_SIGNAL_DELETE, DW_SIGNAL_FUNC(message_deleteevent), NULL);

	dw_window_show(window);

	return TRUE;
}

/* User wants to compose a message */
int DWSIGNAL new_message(HWND hwnd, void *data)
{
	return message_dialog(NULL, FALSE, FALSE, NULL);
}


void reply_dialog(HWND hwnd, MailItem *mi, int reply)
{
	int plug = DW_POINTER_TO_INT(dw_window_get_data(hwndContainer, "plug"));

	if(mi && mi->Type == DATA_TYPE_ITEM)
	{
		unsigned long len;
		char *mytext = plugin_list[plug].getmail(mi->Acc, mi->Folder, mi, &len);

		if(mytext)
		{
			MailParsed *mp = malloc(sizeof(MailParsed));

			char *view = parse_message(mytext, len, mp);

			message_dialog(mp, FALSE, reply, view);
			plugin_list[plug].free(mi->Acc, mytext);
		}
	}
}

/* Tree context handlers */
int DWSIGNAL empty_trash(HWND hwnd, void *data)
{
	MailFolder *mf = (MailFolder *)data;
	int n = 0, plug = DW_POINTER_TO_INT(dw_window_get_data(hwndContainer, "plug"));
	MailItem *mi = plugin_list[plug].getitems(mf->Acc, mf);
	HWND mle = (HWND)dw_window_get_data(hwndContainer, "mle");

	if(mi)
	{
		if(mle)
			dw_mle_clear(mle);

		dw_container_clear(hwndContainer, TRUE);
		while(mi[n].Id)
		{
			plugin_list[plug].delitem(mi[n].Acc, mi[n].Folder, &mi[n]);
			n++;
		}
		setfoldercount(mf, 0);
	}
	return TRUE;
}

int DWSIGNAL new_folder(HWND hwnd, void *data)
{
	return TRUE;
}

/* User wants to compose a message reply */
int DWSIGNAL reply_message(HWND hwnd, void *data)
{
	reply_dialog(hwnd, (MailItem *)data, IDM_REPLY);
	return TRUE;
}

int DWSIGNAL replyq_message(HWND hwnd, void *data)
{
	reply_dialog(hwnd, (MailItem *)data, IDM_REPLYQ);
	return TRUE;
}

int DWSIGNAL replyall_message(HWND hwnd, void *data)
{
	reply_dialog(hwnd, (MailItem *)data, IDM_REPLYALL);
	return TRUE;
}

int DWSIGNAL replyallq_message(HWND hwnd, void *data)
{
	reply_dialog(hwnd, (MailItem *)data, IDM_REPLYALLQ);
	return TRUE;
}

ULONG mainItems[]=
{
	NEWTAB      , REMOVETAB   , TB_SEPARATOR, CONNECT     , DISCONNECT  ,
	TB_SEPARATOR, IDM_EXIT    , TB_SEPARATOR, REMOVEFROMQ , ADDTOQ      ,
	FLUSHQ      , TB_SEPARATOR, SAVETITLE   , UNSAVETITLE , TB_SEPARATOR,
	PB_CHANGE   , TB_SEPARATOR, IDM_PREFERENCES , ADMIN       , TB_SEPARATOR,
	IDM_GENERALHELP, IDM_ABOUT   , 0
};

char mainHelpItems[][100]=
{
	"Check Mail", "Send Mail", "", "Connect", "Disconnect",
	"", "Exit", "", "Remove From Queue", "Add to Queue",
	"Flush Queue", "", "Save Title", "Unsave Title", "",
	"Refresh", "", "Preferences", "Site Administration", "",
	"General Help", "About"
};

void *mainFunctions[] = { (void *)check_mail, (void *)send_mail, NULL, NULL, NULL,
						  NULL, NULL, NULL, NULL, NULL,
						  NULL, NULL, NULL, NULL, NULL,
						  NULL, NULL, NULL, NULL, NULL,
						  NULL, (void *)abouttab };

/* Create the main window with a notebook and a menu. */
void dmail_init(void)
{
	HWND mainbox, toolbox, tempbutton, menuitem, controlbox, mle, splitbar, vsplitbar;
	HMENUI menu;
	float pos;
	ULONG flStyle = DW_FCF_SYSMENU | DW_FCF_TITLEBAR | DW_FCF_SIZEBORDER | DW_FCF_MINMAX |
		DW_FCF_SHELLPOSITION | DW_FCF_TASKLIST | DW_FCF_DLGBORDER;
	char *titles[7] = { "",	"", "Topic", "From", "Date", "Time", "Size" };
	unsigned long flags[7] = {
		DW_CFA_BITMAPORICON | DW_CFA_RIGHT | DW_CFA_HORZSEPARATOR | DW_CFA_SEPARATOR,
		DW_CFA_BITMAPORICON | DW_CFA_RIGHT | DW_CFA_HORZSEPARATOR | DW_CFA_SEPARATOR,
		DW_CFA_STRING | DW_CFA_LEFT | DW_CFA_HORZSEPARATOR,
		DW_CFA_STRING | DW_CFA_LEFT | DW_CFA_HORZSEPARATOR | DW_CFA_SEPARATOR,
		DW_CFA_DATE | DW_CFA_RIGHT | DW_CFA_HORZSEPARATOR | DW_CFA_SEPARATOR,
		DW_CFA_TIME | DW_CFA_RIGHT | DW_CFA_HORZSEPARATOR | DW_CFA_SEPARATOR,
		DW_CFA_ULONG | DW_CFA_RIGHT | DW_CFA_HORZSEPARATOR | DW_CFA_SEPARATOR };
	int z = 0;

	hwndFrame = dw_window_new(HWND_DESKTOP, "DynamicMail", flStyle);

	dw_window_set_icon(hwndFrame, DW_RESOURCE(MAIN_FRAME));

	mainbox = dw_box_new(BOXVERT, 0);

	dw_box_pack_start(hwndFrame, mainbox, 0, 0, TRUE, TRUE, 0);

	toolbox = dw_box_new(BOXHORZ, 1);

	dw_box_pack_start(mainbox, toolbox, 0, 0, TRUE, FALSE, 0);

	/* Create the toolbar */
	while(mainItems[z])
	{
		if(mainItems[z] != -1)
		{
			tempbutton = dw_bitmapbutton_new(mainHelpItems[z], mainItems[z]);
            dw_window_set_style(tempbutton, DW_BS_NOBORDER, DW_BS_NOBORDER);

			if(messageFunctions[z])
				dw_signal_connect(tempbutton, DW_SIGNAL_CLICKED, DW_SIGNAL_FUNC(mainFunctions[z]), (void *)mainItems[z]);

			dw_box_pack_start(toolbox, tempbutton, 30, 30, FALSE, FALSE, 0);
		}
		else
			dw_box_pack_start(toolbox, 0, 5, 30, FALSE, FALSE, 0);
		z++;
	}
	dw_box_pack_start(toolbox, 0, 3, 30, TRUE, FALSE, 0);

	hwndTree = dw_tree_new(1050L);

	dw_signal_connect(hwndTree, DW_SIGNAL_ITEM_SELECT, DW_SIGNAL_FUNC(tree_select), NULL);

	hwndContainer = dw_container_new(117L, TRUE);

	if(dw_container_setup(hwndContainer, flags, titles, 7, 3))
		dw_messagebox("DynamicMail", DW_MB_OK | DW_MB_ERROR, "Error Creating Container!");

	dw_container_set_column_width(hwndContainer, 0, 18);
	dw_container_set_column_width(hwndContainer, 1, 18);
	dw_container_set_column_width(hwndContainer, 2, 300);

	dw_signal_connect(hwndContainer, DW_SIGNAL_ITEM_ENTER, DW_SIGNAL_FUNC(containerselect), NULL);
	dw_signal_connect(hwndContainer, DW_SIGNAL_ITEM_CONTEXT, DW_SIGNAL_FUNC(containercontextmenu), NULL);
	dw_signal_connect(hwndContainer, DW_SIGNAL_ITEM_SELECT, DW_SIGNAL_FUNC(containerfocus), NULL);
	dw_signal_connect(hwndTree, DW_SIGNAL_ITEM_CONTEXT, DW_SIGNAL_FUNC(treecontextmenu), NULL);

	mle = dw_mle_new(1040);

	dw_window_set_data(hwndContainer, "mle", (void *)mle);

	dw_mle_set_word_wrap(mle, TRUE);
	dw_mle_set_editable(mle, FALSE);

	vsplitbar = dw_splitbar_new(BOXVERT, hwndContainer, mle, 0);
	if(config.vsplit)
	{
		pos = (float)config.vsplit;
		dw_splitbar_set(vsplitbar, pos);
	}
	else
		dw_splitbar_set(vsplitbar, 30.0);

	splitbar = dw_splitbar_new(BOXHORZ, hwndTree, vsplitbar, 0);
	if(config.hsplit)
	{
		pos = (float)config.hsplit;
		dw_splitbar_set(splitbar, pos);
	}
	else
		dw_splitbar_set(splitbar, 15.0);

	dw_window_set_data(hwndFrame, "hsplit", (void *)splitbar);
	dw_window_set_data(hwndFrame, "vsplit", (void *)vsplitbar);

	dw_box_pack_start(mainbox, splitbar, 1, 1, TRUE, TRUE, 0);

	controlbox = dw_box_new(BOXHORZ, 0);

	dw_box_pack_start(mainbox, controlbox, 0, 0, TRUE, FALSE, 0);

	stext1 = dw_status_text_new("Welcome to DynamicMail.", 1026);

	dw_window_set_style(stext1, DW_DT_VCENTER, DW_DT_VCENTER);

	dw_box_pack_start(controlbox, stext1, 20, -1, TRUE, FALSE, 2);

	stext2 = dw_status_text_new("Receive idle.", 1026);

	dw_window_set_style(stext2, DW_DT_VCENTER, DW_DT_VCENTER);

	dw_box_pack_start(controlbox, stext2, 20, -1, TRUE, FALSE, 2);

	stext3 = dw_status_text_new("Send idle.", 1026);

	dw_window_set_style(stext3, DW_DT_VCENTER, DW_DT_VCENTER);

	dw_box_pack_start(controlbox, stext3, 20, -1, TRUE, FALSE, 2);

	menubar = dw_menubar_new(hwndFrame);

	menu = dw_menu_new(0L);

	menuitem = dw_menu_append_item(menu, "~Check Mail", IDM_CHECK, 0L, TRUE, FALSE, DW_NOMENU);
	dw_signal_connect(menuitem, DW_SIGNAL_CLICKED, DW_SIGNAL_FUNC(check_mail), NULL);

	menuitem = dw_menu_append_item(menu, "~Send Mail", IDM_SENDMAIL, 0L, TRUE, FALSE, DW_NOMENU);
	dw_signal_connect(menuitem, DW_SIGNAL_CLICKED, DW_SIGNAL_FUNC(send_mail), NULL);

	dw_menu_append_item(menu, "", 0L, 0L, TRUE, FALSE, DW_NOMENU);

	menuitem = dw_menu_append_item(menu, "~Exit", IDM_EXIT, 0L, TRUE, FALSE, DW_NOMENU);
	dw_signal_connect(menuitem, DW_SIGNAL_CLICKED, DW_SIGNAL_FUNC(exitfunc), NULL);

	dw_menu_append_item(menubar, "~File", IDM_FILE, 0L, TRUE, FALSE, menu);

	menu = dw_menu_new(0L);

	menuitem = dw_menu_append_item(menu, locale_string("New Message", 20), IDM_NEWMESS, 0L, TRUE, FALSE, DW_NOMENU);
	dw_signal_connect(menuitem, DW_SIGNAL_CLICKED, DW_SIGNAL_FUNC(new_message), NULL);

	menuitem = dw_menu_append_item(menubar, locale_string("~Messages", 2), IDM_MESSAGE, 0L, TRUE, FALSE, menu);

	menu = dw_menu_new(0L);

	menuitem = dw_menu_append_item(menu, locale_string("Save Position", 20), IDM_SAVEPOS, 0L, TRUE, FALSE, DW_NOMENU);
	dw_signal_connect(menuitem, DW_SIGNAL_CLICKED, DW_SIGNAL_FUNC(save_position), NULL);

	menuitem = dw_menu_append_item(menubar, locale_string("~Tools", 2), IDM_TOOLS, 0L, TRUE, FALSE, menu);

	menu = dw_menu_new(0L);

	menuitem = dw_menu_append_item(menu, locale_string("~General Help", 20), IDM_GENERALHELP, 0L, TRUE, FALSE, DW_NOMENU);

	dw_menu_append_item(menu, "", 0L, 0L, TRUE, FALSE, DW_NOMENU);
	menuitem = dw_menu_append_item(menu, locale_string("~About", 23), IDM_ABOUT, 0L, TRUE, FALSE, DW_NOMENU);

	menuitem = dw_menu_append_item(menubar, locale_string("~Help", 2), IDM_HELP, 0L, TRUE, FALSE, menu);
}

/* Save settings */
void saveconfig(void)
{
	FILE *fp;

	if((fp = fopen("dmail.ini", "w")) != NULL)
	{
		int z = 0;

		while(config.plugins[z] && z < PLUGIN_MAX)
		{
			fprintf(fp, "PLUGIN=%s\n", config.plugins[z]);
			z++;
		}
		z = 0;
		while(config.accounts[z] && z < ACCOUNT_MAX)
		{
			fprintf(fp, "ACCOUNT=%s\n", config.accounts[z]);
			z++;
		}
		fprintf(fp, "X=%d\n", (int)config.x);
		fprintf(fp, "Y=%d\n", (int)config.y);
		fprintf(fp, "WIDTH=%d\n", (int)config.width);
		fprintf(fp, "HEIGHT=%d\n", (int)config.height);
		fprintf(fp, "HSPLIT=%d\n", (int)config.hsplit);
		fprintf(fp, "VSPLIT=%d\n", (int)config.vsplit);
		fclose(fp);
	}
}

/* Load settings */
void loadconfig(void)
{
	FILE *fp;
	char entry[256], entrydata[256];

	if((fp = fopen("dmail.ini", "r")) != NULL)
	{
		int account = 0, plugin = 0;

		while(!feof(fp))
		{
			dmail_getline(fp, entry, entrydata);
			if(strcasecmp(entry, "plugin")==0 && plugin < PLUGIN_MAX)
			{
				config.plugins[plugin] = strdup(entrydata);
				plugin++;
			}
			if(strcasecmp(entry, "account")==0 && plugin < PLUGIN_MAX)
			{
				config.accounts[account] = strdup(entrydata);
				account++;
			}
			if(strcasecmp(entry, "x")==0)
				config.x = atoi(entrydata);
			if(strcasecmp(entry, "y")==0)
				config.y = atoi(entrydata);
			if(strcasecmp(entry, "width")==0)
				config.width = atoi(entrydata);
			if(strcasecmp(entry, "height")==0)
				config.height = atoi(entrydata);
			if(strcasecmp(entry, "hsplit")==0)
				config.hsplit = atoi(entrydata);
			if(strcasecmp(entry, "vsplit")==0)
				config.vsplit = atoi(entrydata);
		}
		fclose(fp);
	}
}

/* The main entry point.  Notice we don't use WinMain() on Windows */
int main(int argc, char *argv[])
{
	int cx, cy, rc, z = 0;

	dw_init(TRUE, argc, argv);

#ifdef __MAC__
	dw_font_set_default("10.Geneva");
#endif
    
	sockinit();

	srand(time(NULL));

	cx = dw_screen_width();
	cy = dw_screen_height();

	mutex = dw_mutex_new();

	loadconfig();

	dmail_init();

	fileicon = dw_icon_load(0,FILEICON);
	foldericon = dw_icon_load(0,FOLDERICON);
	linkicon = dw_icon_load(0,LINKICON);

	/* If there is no config file... then automatically
	 * load the minimal plugin so the program won't be
	 * broken by default.
	 */
	if(!config.plugins[0])
		config.plugins[0] = strdup("minimal");

	while(config.plugins[z] && z < PLUGIN_MAX)
        {
		if(load_backend(config.plugins[z]))
			dw_messagebox("DynamicMail", DW_MB_OK | DW_MB_ERROR, "Could not load backend \"%s\"!", config.plugins[z]);
		z++;
	}

	dmail_init_tree();

	if(hwndFrame)
	{
		dw_window_set_icon(hwndFrame, DW_RESOURCE(MAIN_FRAME));

		dw_signal_connect(hwndFrame, DW_SIGNAL_DELETE, DW_SIGNAL_FUNC(exitfunc), NULL);
		dw_signal_connect(DW_DESKTOP, DW_SIGNAL_DELETE, DW_SIGNAL_FUNC(exitfunc), NULL);

		/* Reload old position if possible */
		if(config.width && config.height)
		{
			dw_window_set_pos_size(hwndFrame, config.x, config.y,
								   config.width, config.height);
		}
		else
		{
			dw_window_set_pos_size(hwndFrame, cx / 8,
								   cy / 8,
								   (cx / 4) * 3,
								   (cy / 4) * 3);
		}

		dw_window_show(hwndFrame);

		dw_main();

		dw_window_destroy(hwndFrame);
	}

	sockshutdown();

	dw_icon_free(fileicon);
	dw_icon_free(foldericon);
	dw_icon_free(linkicon);

	dw_mutex_close(mutex);

	return 0;
}
