#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "dw.h"
#include "userent.h"
#include "backend.h"
#include "dmail.h"

HWND in_account = 0;
DWDialog *dialog;

/* Handle OK press on the account dialog. */
void DWSIGNAL account_ok(HWND window, void *data)
{
	UserEntry *param = (UserEntry *)data;

	if(param)
	{
		HWND *handles = (HWND *)param->data;
		AccountSettings *as = (AccountSettings *)param->data2;

		if(handles && as)
		{
			char *tmp;

			tmp = dw_window_get_text(handles[0]);
			if(tmp)
			{
				strncpy(as->AccountName, tmp, ACCOUNTNAME_MAX);
				dw_free(tmp);
			}
			tmp = dw_window_get_text(handles[1]);
			if(tmp)
			{
				strncpy(as->UserEmail, tmp, HOSTNAME_MAX + USERNAME_MAX);
				dw_free(tmp);
			}
			tmp = dw_window_get_text(handles[2]);
			if(tmp)
			{
				strncpy(as->UserRealName, tmp, REALNAME_MAX);
				dw_free(tmp);
			}
			tmp = dw_window_get_text(handles[3]);
			if(tmp)
			{
				strncpy(as->ReplyEmail, tmp, HOSTNAME_MAX + USERNAME_MAX);
				dw_free(tmp);
			}
			tmp = dw_window_get_text(handles[4]);
			if(tmp)
			{
				strncpy(as->ReplyRealName, tmp, REALNAME_MAX);
				dw_free(tmp);
			}
			tmp = dw_window_get_text(handles[5]);
			if(tmp)
			{
				strncpy(as->RecvHostName, tmp, HOSTNAME_MAX);
				dw_free(tmp);
			}
			tmp = dw_window_get_text(handles[6]);
			if(tmp)
			{
				as->RecvHostPort = atoi(tmp);
				dw_free(tmp);
			}
			tmp = dw_window_get_text(handles[7]);
			if(tmp)
			{
				strncpy(as->RecvHostUser, tmp, USERNAME_MAX);
				dw_free(tmp);
			}
			tmp = dw_window_get_text(handles[8]);
			if(tmp)
			{
				strncpy(as->RecvHostPass, tmp, USERNAME_MAX);
				dw_free(tmp);
			}
			tmp = dw_window_get_text(handles[9]);
			if(tmp)
			{
				strncpy(as->SendHostName, tmp, HOSTNAME_MAX);
				dw_free(tmp);
			}
			tmp = dw_window_get_text(handles[10]);
			if(tmp)
			{
				as->SendHostPort = atoi(tmp);
				dw_free(tmp);
			}
			tmp = dw_window_get_text(handles[11]);
			if(tmp)
			{
				strncpy(as->SendHostUser, tmp, USERNAME_MAX);
				dw_free(tmp);
			}
			tmp = dw_window_get_text(handles[12]);
			if(tmp)
			{
				strncpy(as->SendHostPass, tmp, USERNAME_MAX);
				dw_free(tmp);
			}

			as->RecvHostType = HOST_TYPE_POP3;
			as->SendHostType = HOST_TYPE_SMTP;
		}

		dw_window_destroy(param->window);
		if(param->busy)
			*param->busy = 0;
		if(param->data)
			free(param->data);
		free(param);

		if(dialog)
			dw_dialog_dismiss(dialog, (void *)1);
	}
}

/* Delete event */
int DWSIGNAL confdeleteevent(HWND hwnd, void *data)
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
	if(dialog)
		dw_dialog_dismiss(dialog, NULL);
	return TRUE;
}

void account_dialog_core(AccountSettings *as, int newaccount, int modal)
{
	HWND entrywindow, cancelbutton, okbutton, buttonbox, linebox, xbox, stext, groupbox,
	notebook, notebookbox, *handles = malloc(35 * sizeof(HWND));
	UserEntry *param = malloc(sizeof(UserEntry));
	ULONG flStyle = DW_FCF_SYSMENU | DW_FCF_TITLEBAR | DW_FCF_SIZEBORDER | DW_FCF_MINMAX |
		DW_FCF_SHELLPOSITION | DW_FCF_TASKLIST | DW_FCF_DLGBORDER;
	int pageid, firstpage;
	char tmpbuf[50];

	if(in_account)
	{
		free(handles);
		free(param);
		dw_window_show(in_account);
		return;
	}

	in_account = entrywindow = dw_window_new(HWND_DESKTOP, "Account Settings", flStyle);

	dw_window_set_icon(entrywindow, (HICN)MAIN_FRAME);

	xbox = dw_box_new(BOXVERT, 3);

	dw_box_pack_start(entrywindow, xbox, 150, 70, TRUE, TRUE, 0);

	notebook = dw_notebook_new(1050L, TRUE);

	dw_box_pack_start(xbox, notebook, 150, 300, TRUE, TRUE, 0);

	/* General Settings */
	notebookbox = dw_box_new(BOXVERT, 8);

	stext = dw_box_new(BOXHORZ, 0);

	dw_box_pack_start(notebookbox, stext, 0, 0, TRUE, TRUE, 0);

	groupbox = stext;

	linebox = dw_box_new(BOXVERT, 4);

	dw_box_pack_start(groupbox, linebox, 0, 0, TRUE, TRUE, 0);

	stext = dw_text_new("Description", 0);
	dw_window_set_style(stext, DW_DT_VCENTER, DW_DT_VCENTER);

	dw_box_pack_start(linebox, stext, 30, 20, TRUE, TRUE, 0);

	linebox = dw_box_new(BOXVERT, 4);

	dw_box_pack_start(groupbox, linebox, 0, 0, TRUE, TRUE, 0);

	handles[0] = dw_entryfield_new("", 0);
	dw_entryfield_set_limit(handles[0], ACCOUNTNAME_MAX);
	dw_window_set_text(handles[0], as->AccountName);

	dw_box_pack_start(linebox, handles[0], 100, 20, TRUE, TRUE, 0);

	groupbox = dw_groupbox_new(BOXVERT, 7, "From Information");

	dw_box_pack_start(notebookbox, groupbox, 0, 0, TRUE, TRUE, 0);

	stext = dw_box_new(BOXHORZ, 0);

	dw_box_pack_start(groupbox, stext, 0, 0, TRUE, TRUE, 0);

	groupbox = stext;

	linebox = dw_box_new(BOXVERT, 4);

	dw_box_pack_start(groupbox, linebox, 0, 0, TRUE, TRUE, 0);

	stext = dw_text_new("E-Mail Address", 0);
	dw_window_set_style(stext, DW_DT_VCENTER, DW_DT_VCENTER);

	dw_box_pack_start(linebox, stext, 30, 20, TRUE, TRUE, 0);

	stext = dw_text_new("Real Name", 0);
	dw_window_set_style(stext, DW_DT_VCENTER, DW_DT_VCENTER);

	dw_box_pack_start(linebox, stext, 30, 20, TRUE, TRUE, 0);

	linebox = dw_box_new(BOXVERT, 4);

	dw_box_pack_start(groupbox, linebox, 0, 0, TRUE, TRUE, 0);

	handles[1] = dw_entryfield_new("", 0);
	dw_entryfield_set_limit(handles[1], HOSTNAME_MAX + USERNAME_MAX);
	dw_window_set_text(handles[1], as->UserEmail);

	dw_box_pack_start(linebox, handles[1], 100, 20, TRUE, TRUE, 0);

	handles[2] = dw_entryfield_new("", 0);
	dw_entryfield_set_limit(handles[2], REALNAME_MAX);
	dw_window_set_text(handles[2], as->UserRealName);

	dw_box_pack_start(linebox, handles[2], 100, 20, TRUE, TRUE, 0);

	groupbox = dw_groupbox_new(BOXVERT, 7, "Reply-To Information");

	dw_box_pack_start(notebookbox, groupbox, 0, 0, TRUE, TRUE, 0);

	stext = dw_box_new(BOXHORZ, 0);

	dw_box_pack_start(groupbox, stext, 0, 0, TRUE, TRUE, 0);

	groupbox = stext;

	linebox = dw_box_new(BOXVERT, 4);

	dw_box_pack_start(groupbox, linebox, 0, 0, TRUE, TRUE, 0);

	stext = dw_text_new("E-Mail Address", 0);
	dw_window_set_style(stext, DW_DT_VCENTER, DW_DT_VCENTER);

	dw_box_pack_start(linebox, stext, 30, 20, TRUE, TRUE, 0);

	stext = dw_text_new("Real Name", 0);
	dw_window_set_style(stext, DW_DT_VCENTER, DW_DT_VCENTER);

	dw_box_pack_start(linebox, stext, 30, 20, TRUE, TRUE, 0);

	linebox = dw_box_new(BOXVERT, 4);

	dw_box_pack_start(groupbox, linebox, 0, 0, TRUE, TRUE, 0);

	handles[3] = dw_entryfield_new("", 0);
	dw_entryfield_set_limit(handles[3], HOSTNAME_MAX + USERNAME_MAX);
	dw_window_set_text(handles[3], as->ReplyEmail);

	dw_box_pack_start(linebox, handles[3], 100, 20, TRUE, TRUE, 0);

	handles[4] = dw_entryfield_new("", 0);
	dw_entryfield_set_limit(handles[4], REALNAME_MAX);
	dw_window_set_text(handles[4], as->ReplyRealName);

	dw_box_pack_start(linebox, handles[4], 100, 20, TRUE, TRUE, 0);

	dw_box_pack_start(notebookbox, 0, 100, 100, TRUE, TRUE, 0);

	firstpage = pageid = dw_notebook_page_new(notebook, 0L, FALSE);

	dw_notebook_pack(notebook, pageid, notebookbox);

	/* Due to a GTK limitiation the page text must be set after the page is packed */
	dw_notebook_page_set_text(notebook, pageid, "General");
	dw_notebook_page_set_status_text(notebook, pageid, "General Account Settings");

	/* Receive Settings */
	notebookbox = dw_box_new(BOXVERT, 8);

	groupbox = dw_groupbox_new(BOXVERT, 7, "POP3");

	dw_box_pack_start(notebookbox, groupbox, 0, 0, TRUE, TRUE, 0);

	stext = dw_box_new(BOXHORZ, 0);

	dw_box_pack_start(groupbox, stext, 0, 0, TRUE, TRUE, 0);

	groupbox = stext;

	linebox = dw_box_new(BOXVERT, 4);

	dw_box_pack_start(groupbox, linebox, 0, 0, TRUE, TRUE, 0);

	stext = dw_text_new("POP3 Server", 0);
	dw_window_set_style(stext, DW_DT_VCENTER, DW_DT_VCENTER);

	dw_box_pack_start(linebox, stext, 30, 20, TRUE, TRUE, 0);

	stext = dw_text_new("Port", 0);
	dw_window_set_style(stext, DW_DT_VCENTER, DW_DT_VCENTER);

	dw_box_pack_start(linebox, stext, 30, 20, TRUE, TRUE, 0);

	stext = dw_text_new("Username", 0);
	dw_window_set_style(stext, DW_DT_VCENTER, DW_DT_VCENTER);

	dw_box_pack_start(linebox, stext, 30, 20, TRUE, TRUE, 0);

	stext = dw_text_new("Password", 0);
	dw_window_set_style(stext, DW_DT_VCENTER, DW_DT_VCENTER);

	dw_box_pack_start(linebox, stext, 30, 20, TRUE, TRUE, 0);

	linebox = dw_box_new(BOXVERT, 4);

	dw_box_pack_start(groupbox, linebox, 0, 0, TRUE, TRUE, 0);

	handles[5] = dw_entryfield_new("", 0);
	dw_entryfield_set_limit(handles[5], HOSTNAME_MAX);
	dw_window_set_text(handles[5], as->RecvHostName);

	dw_box_pack_start(linebox, handles[5], 100, 20, TRUE, TRUE, 0);

	handles[6] = dw_entryfield_new("", 0);
	dw_entryfield_set_limit(handles[6], REALNAME_MAX);
	sprintf(tmpbuf, "%d", as->RecvHostPort ? as->RecvHostPort : 110);
	dw_window_set_text(handles[6], tmpbuf);

	dw_box_pack_start(linebox, handles[6], 100, 20, TRUE, TRUE, 0);

	handles[7] = dw_entryfield_new("", 0);
	dw_entryfield_set_limit(handles[7], USERNAME_MAX);
	dw_window_set_text(handles[7], as->RecvHostUser);

	dw_box_pack_start(linebox, handles[7], 100, 20, TRUE, TRUE, 0);

	handles[8] = dw_entryfield_password_new("", 0);
	dw_entryfield_set_limit(handles[8], USERNAME_MAX);
	dw_window_set_text(handles[8], as->RecvHostPass);

	dw_box_pack_start(linebox, handles[8], 100, 20, TRUE, TRUE, 0);

	dw_box_pack_start(notebookbox, 0, 100, 160, TRUE, TRUE, 0);

	pageid = dw_notebook_page_new(notebook, 0L, FALSE);

	dw_notebook_pack(notebook, pageid, notebookbox);

	/* Due to a GTK limitiation the page text must be set after the page is packed */
	dw_notebook_page_set_text(notebook, pageid, "Receive");
	dw_notebook_page_set_status_text(notebook, pageid, "Account Receive Settings");

	/* Send Settings */
	notebookbox = dw_box_new(BOXVERT, 8);

	groupbox = dw_groupbox_new(BOXVERT, 7, "SMTP");

	dw_box_pack_start(notebookbox, groupbox, 0, 0, TRUE, TRUE, 0);

	stext = dw_box_new(BOXHORZ, 0);

	dw_box_pack_start(groupbox, stext, 0, 0, TRUE, TRUE, 0);

	groupbox = stext;

	linebox = dw_box_new(BOXVERT, 4);

	dw_box_pack_start(groupbox, linebox, 0, 0, TRUE, TRUE, 0);

	stext = dw_text_new("SMTP Server", 0);
	dw_window_set_style(stext, DW_DT_VCENTER, DW_DT_VCENTER);

	dw_box_pack_start(linebox, stext, 30, 20, TRUE, TRUE, 0);

	stext = dw_text_new("Port", 0);
	dw_window_set_style(stext, DW_DT_VCENTER, DW_DT_VCENTER);

	dw_box_pack_start(linebox, stext, 30, 20, TRUE, TRUE, 0);

	stext = dw_text_new("Username", 0);
	dw_window_set_style(stext, DW_DT_VCENTER, DW_DT_VCENTER);

	dw_box_pack_start(linebox, stext, 30, 20, TRUE, TRUE, 0);

	stext = dw_text_new("Password", 0);
	dw_window_set_style(stext, DW_DT_VCENTER, DW_DT_VCENTER);

	dw_box_pack_start(linebox, stext, 30, 20, TRUE, TRUE, 0);

	linebox = dw_box_new(BOXVERT, 4);

	dw_box_pack_start(groupbox, linebox, 0, 0, TRUE, TRUE, 0);

	handles[9] = dw_entryfield_new("", 0);
	dw_entryfield_set_limit(handles[9], HOSTNAME_MAX);
	dw_window_set_text(handles[9], as->SendHostName);

	dw_box_pack_start(linebox, handles[9], 100, 20, TRUE, TRUE, 0);

	handles[10] = dw_entryfield_new("", 0);
	dw_entryfield_set_limit(handles[10], REALNAME_MAX);
	sprintf(tmpbuf, "%d", as->RecvHostPort ? as->SendHostPort : 25);
	dw_window_set_text(handles[10], tmpbuf);

	dw_box_pack_start(linebox, handles[10], 100, 20, TRUE, TRUE, 0);

	handles[11] = dw_entryfield_new("", 0);
	dw_entryfield_set_limit(handles[11], USERNAME_MAX);
	dw_window_set_text(handles[11], as->SendHostUser);

	dw_box_pack_start(linebox, handles[11], 100, 20, TRUE, TRUE, 0);

	handles[12] = dw_entryfield_password_new("", 0);
	dw_entryfield_set_limit(handles[12], USERNAME_MAX);
	dw_window_set_text(handles[12], as->SendHostPass);

	dw_box_pack_start(linebox, handles[12], 100, 20, TRUE, TRUE, 0);

	dw_box_pack_start(notebookbox, 0, 100, 160, TRUE, TRUE, 0);

	pageid = dw_notebook_page_new(notebook, 0L, FALSE);

	dw_notebook_pack(notebook, pageid, notebookbox);

	/* Due to a GTK limitiation the page text must be set after the page is packed */
	dw_notebook_page_set_text(notebook, pageid, "Send");
	dw_notebook_page_set_status_text(notebook, pageid, "Account Send Settings");

	/* Buttons */
	buttonbox = dw_box_new(BOXHORZ, 5);

	dw_box_pack_start(xbox, buttonbox, 0, 0, TRUE, FALSE, 0);

	okbutton = dw_button_new("Ok", 1001L);

	dw_box_pack_start(buttonbox, 0, 40, 30, TRUE, FALSE, 2);
	dw_box_pack_start(buttonbox, okbutton, 40, 30, TRUE, FALSE, 2);

	cancelbutton = dw_button_new("Cancel", 1002L);

	dw_box_pack_start(buttonbox, cancelbutton, 40, 30, TRUE, FALSE, 2);
	dw_box_pack_start(buttonbox, 0, 40, 30, TRUE, FALSE, 2);

	param->window = entrywindow;
	param->filename = NULL;
	param->data = (void *)handles;
	param->data2 = (void *)as;
	param->busy = &in_account;

	if(modal)
		dialog = dw_dialog_new((void *)entrywindow);
	else
		dialog = NULL;

	dw_signal_connect(entrywindow, DW_SIGNAL_DELETE, DW_SIGNAL_FUNC(confdeleteevent), (void *)param);

	dw_signal_connect(okbutton, DW_SIGNAL_CLICKED, DW_SIGNAL_FUNC(account_ok), (void *)param);
	dw_signal_connect(cancelbutton, DW_SIGNAL_CLICKED, DW_SIGNAL_FUNC(confdeleteevent), (void *)param);

	dw_window_default(entrywindow, okbutton);

#ifdef __WIN32__
	dw_window_set_size(entrywindow, 420, 420);
#else
	dw_window_set_size(entrywindow, 460, 480);
#endif

	dw_notebook_page_set(notebook, firstpage);

	dw_window_show(entrywindow);
}

void DWSIGNAL account_dialog(HWND handle, void *data)
{
	AccountSettings *as = findsettings(data);

	if(as)
		account_dialog_core(as, FALSE, FALSE);
}

int modal_account_dialog(AccountSettings *as)
{
	if(in_account)
		return FALSE;

	account_dialog_core(as, TRUE, TRUE);

	return (int)dw_dialog_wait(dialog);
}
