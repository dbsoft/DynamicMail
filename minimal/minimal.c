/*
 * This is a minimal disk based implementation of the
 * mail backend.
 */
#include <stdio.h>
#include <sys/stat.h>
#include <string.h>
#include <dwcompat.h>
#include <dw.h>
#include "backend.h"
#include "minimal.h"

/* Choose a unique ID for this plugin */
#define MINIMAL_BACKEND_ID 123
#define FOLDER_INI_NAME "Folder.ini"
#define MAIL_INI_NAME "Mail.ini"
#define ACCOUNT_INI_NAME "Account.ini"

HMTX backend_mtx;

int API backend_newfolder(Account *acc, MailFolder *mf);

/* Utility function */
void stripcrlf(char *buffer)
{
	int z, len = strlen(buffer);

	for(z=0;z<len;z++)
	{
		if(buffer[z] == '\r' || buffer[z] == '\n')
		{
			buffer[z] = 0;
			return;
		}
	}
}

/* Initialize this backend */
int API backend_init(BackendInfo *bi)
{
	sprintf(bi->Name, "Minimal");
	sprintf(bi->Author, "Brian Smith");

	backend_mtx = dw_mutex_new();

	return MINIMAL_BACKEND_ID;
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
	char *namebuf;
	AccountStruct *as = malloc(sizeof(AccountStruct));

	dw_mutex_lock(backend_mtx);

	namebuf = malloc(strlen(name) + strlen(FOLDER_INI_NAME) + 2);
	sprintf(namebuf, "%s/%s", name, FOLDER_INI_NAME);

	as->Type = DATA_TYPE_ACCOUNT;
	as->fp = fopen(namebuf, "r+");
	as->name = strdup(name);

	free(namebuf);

	dw_mutex_unlock(backend_mtx);
	return (Account *)as;
}

/* Open a mail account, allocating any needed resources */
Account * API backend_newaccount(char *name)
{
	char *namebuf;
	AccountStruct *as = malloc(sizeof(AccountStruct));
	MailFolder mf;

	dw_mutex_lock(backend_mtx);

	makedir(name);
	namebuf = malloc(strlen(name) + strlen(FOLDER_INI_NAME) + 2);
	sprintf(namebuf, "%s/%s", name, FOLDER_INI_NAME);

	as->Type = DATA_TYPE_ACCOUNT;
	as->fp = fopen(namebuf, "w+");
	as->name = strdup(name);

	free(namebuf);

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
		if(as->fp)
			fclose(as->fp);
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
	DIR *dir;
	struct dirent *ent;
	struct dwstat bleah;

	if(!(dir = opendir(".")))
		return NULL;

	while((ent = readdir(dir)) != 0)
	{
		stat(ent->d_name, &bleah);

		if(S_ISDIR(bleah.st_mode))
		{
			char *namebuf = malloc(strlen(ent->d_name) + strlen(FOLDER_INI_NAME) + 2);

			sprintf(namebuf, "%s/%s", ent->d_name, FOLDER_INI_NAME);

			if(!stat(namebuf, &bleah))
				counter++;

			free(namebuf);
		}
	}
	rewinddir(dir);
	if(counter)
	{
		ret = calloc(counter+1, sizeof(char *));

		counter = 0;

		while((ent = readdir(dir)) != 0)
		{
			stat(ent->d_name, &bleah);

			if(S_ISDIR(bleah.st_mode))
			{
				char *namebuf = malloc(strlen(ent->d_name) + strlen(FOLDER_INI_NAME) + 2);

				sprintf(namebuf, "%s/%s", ent->d_name, FOLDER_INI_NAME);

				if(!stat(namebuf, &bleah))
				{
					ret[counter] = strdup(ent->d_name);
					counter++;
				}

				free(namebuf);
			}
		}
	}

	closedir(dir);
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
	char *namebuf;
	FILE *fp;

	if(!as || !set)
		return;

	dw_mutex_lock(backend_mtx);
	set->Type = DATA_TYPE_SETTINGS;

	namebuf = malloc(strlen(as->name) + strlen(ACCOUNT_INI_NAME) + 2);
	sprintf(namebuf, "%s/" ACCOUNT_INI_NAME, as->name);
	if((fp = fopen(namebuf, "w")) != NULL)
	{
		fprintf(fp, "RECVHOSTTYPE=%d\n", set->RecvHostType);
		fprintf(fp, "RECVHOSTNAME=%s\n", set->RecvHostName);
		fprintf(fp, "RECVHOSTPORT=%d\n", set->RecvHostPort);
		fprintf(fp, "RECVHOSTUSER=%s\n", set->RecvHostUser);
		fprintf(fp, "RECVHOSTPASS=%s\n", set->RecvHostPass);
		fprintf(fp, "SENDHOSTTYPE=%d\n", set->SendHostType);
		fprintf(fp, "SENDHOSTNAME=%s\n", set->SendHostName);
		fprintf(fp, "SENDHOSTPORT=%d\n", set->SendHostPort);
		fprintf(fp, "SENDHOSTUSER=%s\n", set->SendHostUser);
		fprintf(fp, "SENDHOSTPASS=%s\n", set->SendHostPass);
		fprintf(fp, "USEREMAIL=%s\n",    set->UserEmail);
		fprintf(fp, "USERREALNAME=%s\n", set->UserRealName);
		fprintf(fp, "REPLYEMAIL=%s\n",   set->ReplyEmail);
		fprintf(fp, "REPLYREALNAME=%s\n",set->ReplyRealName);
		fclose(fp);
	}
	free(namebuf);
	dw_mutex_unlock(backend_mtx);
	return;
}

/* Generic function to parse information from a config file */
void minimal_getline(FILE *f, char *entry, char *entrydata)
{
	char in[256] = {0};
	int z;

	if(fgets(in, 255, f))
	{
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
	}
	strcpy(entry, "");
	strcpy(entrydata, "");
}

/* Load settings associated with an account */
void API backend_getsettings(Account *acc, AccountSettings *set)
{
	AccountStruct *as = (AccountStruct *)acc;
	char entry[256], entrydata[256], *namebuf;
	FILE *fp;

	if(!as || !set)
		return;

	dw_mutex_lock(backend_mtx);
	set->Type = DATA_TYPE_SETTINGS;

	namebuf = malloc(strlen(as->name) + strlen(ACCOUNT_INI_NAME) + 2);
	sprintf(namebuf, "%s/" ACCOUNT_INI_NAME, as->name);
	strncpy(set->AccountName, as->name, ACCOUNTNAME_MAX);
	if((fp = fopen(namebuf, "r")) != NULL)
	{
		while(!feof(fp))
		{
			minimal_getline(fp, entry, entrydata);
			if(strcasecmp(entry, "recvhosttype")==0)
				set->RecvHostType = atoi(entrydata);
			if(strcasecmp(entry, "recvhostname")==0)
				strncpy(set->RecvHostName, entrydata, HOSTNAME_MAX);
			if(strcasecmp(entry, "recvhostport")==0)
				set->RecvHostPort = atoi(entrydata);
			if(strcasecmp(entry, "recvhostuser")==0)
				strncpy(set->RecvHostUser, entrydata, USERNAME_MAX);
			if(strcasecmp(entry, "recvhostpass")==0)
				strncpy(set->RecvHostPass, entrydata, USERNAME_MAX);
			if(strcasecmp(entry, "sendhosttype")==0)
				set->SendHostType = atoi(entrydata);
			if(strcasecmp(entry, "sendhostname")==0)
				strncpy(set->SendHostName, entrydata, HOSTNAME_MAX);
			if(strcasecmp(entry, "sendhostport")==0)
				set->SendHostPort = atoi(entrydata);
			if(strcasecmp(entry, "sendhostuser")==0)
				strncpy(set->SendHostUser, entrydata, USERNAME_MAX);
			if(strcasecmp(entry, "sendhostpass")==0)
				strncpy(set->SendHostPass, entrydata, USERNAME_MAX);
			if(strcasecmp(entry, "useremail")==0)
				strncpy(set->UserEmail, entrydata, HOSTNAME_MAX + USERNAME_MAX);
			if(strcasecmp(entry, "userrealname")==0)
				strncpy(set->UserRealName, entrydata, REALNAME_MAX);
			if(strcasecmp(entry, "replyemail")==0)
				strncpy(set->ReplyEmail, entrydata, HOSTNAME_MAX + USERNAME_MAX);
			if(strcasecmp(entry, "replyrealname")==0)
				strncpy(set->ReplyRealName, entrydata, REALNAME_MAX);
		}
		fclose(fp);
	}
	free(namebuf);
	dw_mutex_unlock(backend_mtx);
	return;
}

void addfolderitem(MailFolder *mf, char *buf, int commas)
{
	switch(commas)
	{
	case 0:
		strcpy(mf->Name, buf);
		break;
	case 1:
		strcpy(mf->Parent, buf);
		break;
	case 2:
		mf->Flags = atoi(buf);
		break;
	}
}

/* Get an array of folders */
MailFolder * API backend_getfolders(Account *acc)
{
	AccountStruct *as = (AccountStruct *)acc;
	MailFolder *mf = NULL;
	char buf[1024];
	int counter = 0;

	if(!as || !as->fp)
		return NULL;

	dw_mutex_lock(backend_mtx);
	/* Find out how many folders we have */
	rewind(as->fp);
	while(!feof(as->fp))
	{
		if(fgets(buf, 1024, as->fp))
		{
			stripcrlf(buf);
			counter++;
		}
	}
	/* Allocate a sufficient return buffer */
	mf = calloc(counter + 1, sizeof(MailFolder));
	rewind(as->fp);
	counter = 0;
	/* Fill in the table */
	while(!feof(as->fp))
	{
		int z, len, commas = 0;
		char *start = buf;

		if(fgets(buf, 1024, as->fp))
		{
			stripcrlf(buf);
			len = strlen(buf);

			for(z=0;z<len;z++)
			{
				if(buf[z] == ',')
				{
					buf[z] = 0;
					addfolderitem(&mf[counter], start, commas);
					start = &buf[z+1];
					commas++;

				}
			}
			addfolderitem(&mf[counter], start, commas);
			mf[counter].Type = DATA_TYPE_FOLDER;
			mf[counter].Acc = acc;
			counter++;
		}
	}
	dw_mutex_unlock(backend_mtx);
	return mf;
}

/* Create a new folder in an account */
int API backend_newfolder(Account *acc, MailFolder *mf)
{
	AccountStruct *as = (AccountStruct *)acc;
	char *namebuf;

	if(!as || !as->fp)
		return 1;

	dw_mutex_lock(backend_mtx);
	fseek(as->fp, 0, SEEK_END);
	fprintf(as->fp, "%s,%s,%d\n", mf->Name, mf->Parent, (int)mf->Flags);

	namebuf = malloc(strlen(as->name) + strlen(mf->Name) + strlen(mf->Parent) + 3);
    if(mf->Parent[0])
		sprintf(namebuf, "%s/%s/%s", as->name, mf->Parent, mf->Name);
	else
		sprintf(namebuf, "%s/%s", as->name, mf->Name);
	makedir(namebuf);
	free(namebuf);

	mf->Type = DATA_TYPE_FOLDER;
	mf->Acc = acc;

	dw_mutex_unlock(backend_mtx);
	return 0;
}

/* Remove a folder from an account */
int API backend_delfolder(Account *acc, MailFolder *mf)
{
	AccountStruct *as = (AccountStruct *)acc;
	char *namebuf;
	FILE *tmp;
	MailFolder *mfs;
	int z = 0;

	if(!as || !as->fp)
		return 1;

	if(!(mfs = backend_getfolders(acc)))
		return 1;

	dw_mutex_lock(backend_mtx);

	/* Remove the entry from the INI file */
	if(!(tmp = fopen("tmpfold.ini", "w")))
		return 1;

	while(mfs[z].Name[0])
	{
		if(strcmp(mfs[z].Name, mf->Name) != 0 && strcmp(mfs[z].Parent, mf->Parent) != 0)
			fprintf(tmp, "%s,%s,%d\n", mfs[z].Name, mfs[z].Parent, (int)mfs[z].Flags);
		z++;
	}
	fclose(tmp);
	free(mfs);
	namebuf = malloc(strlen(as->name) + strlen(FOLDER_INI_NAME) + 2);

	/* Replace the temporary file with the real one
	 * and reopen the handle.
	 */
	sprintf(namebuf, "%s/%s", as->name, FOLDER_INI_NAME);
	remove(namebuf);
	fclose(as->fp);
	rename("tmpfold.ini", namebuf);
	as->fp = fopen(namebuf, "r+");
	free(namebuf);

	/* Delete the actual directory */
	namebuf = malloc(strlen(as->name) + strlen(mf->Name) + strlen(mf->Parent) + 3);
    if(mf->Parent[0])
		sprintf(namebuf, "%s/%s/%s", as->name, mf->Parent, mf->Name);
	else
		sprintf(namebuf, "%s/%s", as->name, mf->Name);
	rmdir(namebuf);
	free(namebuf);

	dw_mutex_unlock(backend_mtx);
	return 0;
}

void addmailitem(MailItem *mi, char *buf, int commas)
{
	switch(commas)
	{
		/* Mail ID */
	case 0:
		mi->Id = atoi(buf);
		break;
		/* Mail Size */
	case 1:
		mi->Size = atoi(buf);
		break;
		/* Mail Date */
	case 2:
		{
			int month = 0, day = 0, year = 0;
			if(buf[0])
				sscanf(buf, "%d-%d-%d", &month, &day, &year);
			mi->Date.month = month;
			mi->Date.day = day;
			mi->Date.year = year;
		}
		break;
		/* Mail Time */
	case 3:
		{
			int hours = 0, minutes = 0, seconds = 0;
			if(buf[0])
				sscanf(buf, "%d:%d:%d", &hours, &minutes, &seconds);
			mi->Time.hours = hours;
			mi->Time.minutes = minutes;
			mi->Time.seconds = seconds;
		}
		break;
		/* Mail Topic */
	case 4:
		strcpy(mi->Topic, buf);
		break;
		/* Mail To */
	case 5:
		strcpy(mi->To, buf);
		break;
		/* Mail From */
	case 6:
		strcpy(mi->From, buf);
		break;
	}
}

/* Get a list of mail entries */
MailItem * API backend_getitems(Account *acc, MailFolder *mf)
{
	AccountStruct *as = (AccountStruct *)acc;
	MailItem *mi = NULL;
	char buf[1024], *namebuf;
	int counter = 0;
	FILE *tmp;

	if(!as || !mf)
		return NULL;

	dw_mutex_lock(backend_mtx);
	/* Find out how many items we have */
	namebuf = malloc(strlen(as->name) + strlen(mf->Name) + strlen(mf->Parent) + strlen(MAIL_INI_NAME) + 4);
    if(mf->Parent[0])
		sprintf(namebuf, "%s/%s/%s/" MAIL_INI_NAME, as->name, mf->Parent, mf->Name);
	else
		sprintf(namebuf, "%s/%s/" MAIL_INI_NAME, as->name, mf->Name);
	if(!(tmp = fopen(namebuf, "r")))
	{
		free(namebuf);
		dw_mutex_unlock(backend_mtx);
		return NULL;
	}
	free(namebuf);
	while(!feof(tmp))
	{
		if(fgets(buf, 1024, tmp))
		{
			stripcrlf(buf);
			counter++;
		}
	}
	/* Allocate a sufficient return buffer */
	mi = calloc(counter + 1, sizeof(MailItem));
	rewind(tmp);
	counter = 0;
	/* Fill in the table */
	while(!feof(tmp))
	{
		int z, len, commas = 0;
		char *start = buf;

		if(fgets(buf, 1024, tmp))
		{
			stripcrlf(buf);
			len = strlen(buf);


			for(z=0;z<len;z++)
			{
				if(buf[z] == '\255')
				{
					buf[z] = 0;
					addmailitem(&mi[counter], start, commas);
					start = &buf[z+1];
					commas++;

				}
			}
			addmailitem(&mi[counter], start, commas);
			mi[counter].Type = DATA_TYPE_ITEM;
			mi[counter].Acc = acc;
			mi[counter].Folder = mf;
			counter++;
		}
	}
	fclose(tmp);
	dw_mutex_unlock(backend_mtx);
	return mi;
}

/* Create a name using base 36 */
void makename(char *buf, int num)
{
	int mynum = num, z;

	for(z=0;z<8;z++)
	{
		int rem = mynum % 36;

		if(rem > -1 && rem < 10)
			buf[7-z] = '0' + rem;
		else if(rem > 9)
			buf[7-z] = 'a' + (rem-10);

		mynum = mynum / 36;

		if(mynum == 0)
			break;
	}
}

/* Create a new mail item */
int API backend_newitem(Account *acc, MailFolder *mf, MailItem *mi)
{
	AccountStruct *as = (AccountStruct *)acc;
	char *namebuf;
	FILE *tmp;
	struct dwstat bleah;
	int pos;

	if(!as || !mf || !mi)
		return 1;

	dw_mutex_lock(backend_mtx);

	/* Start at 1 not 0 */
	if(!mf->LastId)
		mf->LastId = 1;

	namebuf = malloc(strlen(as->name) + strlen(mf->Name) + strlen(mf->Parent) + 16);
	if(mf->Parent[0])
	{
		pos = strlen(as->name) + strlen(mf->Name) + strlen(mf->Parent) + 3;
		sprintf(namebuf, "%s/%s/%s/00000000.msg", as->name, mf->Parent, mf->Name);
	}
	else
	{
		pos = strlen(as->name) + strlen(mf->Name) + 2;
		sprintf(namebuf, "%s/%s/00000000.msg", as->name, mf->Name);
	}

	makename(&namebuf[pos], mf->LastId);

	while(!stat(namebuf, &bleah))
	{
		mf->LastId++;
		makename(&namebuf[pos], mf->LastId);
	}

	mi->Id = mf->LastId;
	mi->Type = DATA_TYPE_ITEM;
	mi->Acc = acc;
	mi->Folder = mf;
	mf->LastId++;
	free(namebuf);

	namebuf = malloc(strlen(as->name) + strlen(mf->Name) + strlen(mf->Parent) + strlen(MAIL_INI_NAME) + 4);
    if(mf->Parent[0])
		sprintf(namebuf, "%s/%s/%s/" MAIL_INI_NAME, as->name, mf->Parent, mf->Name);
	else
		sprintf(namebuf, "%s/%s/" MAIL_INI_NAME, as->name, mf->Name);
	if(!(tmp = fopen(namebuf, "a+")))
	{
		free(namebuf);
		dw_mutex_unlock(backend_mtx);
		return 1;
	}
	free(namebuf);
	fprintf(tmp, "%d\255%d\255%d-%d-%d\255%d:%d:%d\255%s\255%s\255%s\n", (int)mi->Id, (int)mi->Size,
			mi->Date.month, mi->Date.day, mi->Date.year, mi->Time.hours, mi->Time.minutes, mi->Time.seconds,
			mi->Topic, mi->To, mi->From);
	fclose(tmp);
	dw_mutex_unlock(backend_mtx);

	return 0;
}

/* Remove a mail item from a mail folder */
int API backend_delitem(Account *acc, MailFolder *mf, MailItem *mi)
{
	AccountStruct *as = (AccountStruct *)acc;
	char *namebuf;
	FILE *tmp;
	MailItem *mis;
	int pos, z = 0;

	if(!as)
		return 1;

	if(!(mis = backend_getitems(acc, mf)))
		return 1;

	dw_mutex_lock(backend_mtx);

	/* Remove the entry from the INI file */
	if(!(tmp = fopen("tmpmail.ini", "w")))
		return 1;

	while(mis[z].Id)
	{
		if(mi->Id != mis[z].Id)
			fprintf(tmp, "%d\255%d\255%d-%d-%d\255%d:%d:%d\255%s\255%s\255%s\n", (int)mis[z].Id, (int)mis[z].Size,
					mis[z].Date.month, mis[z].Date.day, mis[z].Date.year, mis[z].Time.hours, mis[z].Time.minutes, mis[z].Time.seconds,
					mis[z].Topic, mis[z].To, mis[z].From);
		z++;
	}
	fclose(tmp);
	free(mis);

	/* Replace the temporary file with the real one.
	 */
	namebuf = malloc(strlen(as->name) + strlen(mf->Name) + strlen(mf->Parent) + strlen(MAIL_INI_NAME) + 4);
    if(mf->Parent[0])
		sprintf(namebuf, "%s/%s/%s/" MAIL_INI_NAME, as->name, mf->Parent, mf->Name);
	else
		sprintf(namebuf, "%s/%s/" MAIL_INI_NAME, as->name, mf->Name);
	remove(namebuf);
	rename("tmpmail.ini", namebuf);
	free(namebuf);

	/* Actually delete the message file */
	namebuf = malloc(strlen(as->name) + strlen(mf->Name) + strlen(mf->Parent) + 16);
    if(mf->Parent[0])
	{
		pos = strlen(as->name) + strlen(mf->Name) + strlen(mf->Parent) + 3;
		sprintf(namebuf, "%s/%s/%s/00000000.msg", as->name, mf->Parent, mf->Name);
	}
	else
	{
		pos = strlen(as->name) + strlen(mf->Name) + 2;
		sprintf(namebuf, "%s/%s/00000000.msg", as->name, mf->Name);
	}
	makename(&namebuf[pos], mi->Id);
	remove(namebuf);
	free(namebuf);

	dw_mutex_unlock(backend_mtx);
	return 0;
}

/* Retrieve a raw mail buffer associated with a mail item */
char * API backend_getmail(Account *acc, MailFolder *mf, MailItem *mi, unsigned long *len)
{
	AccountStruct *as = (AccountStruct *)acc;
	char *namebuf, *mailbuf = NULL;
	FILE *tmp;
	struct dwstat bleah;
	int pos;

	if(!as || !mf || !mi || !len)
		return NULL;

	*len = 0;

	dw_mutex_lock(backend_mtx);
	namebuf = malloc(strlen(as->name) + strlen(mf->Name) + strlen(mf->Parent) + 16);
	if(mf->Parent[0])
	{
		pos = strlen(as->name) + strlen(mf->Name) + strlen(mf->Parent) + 3;
		sprintf(namebuf, "%s/%s/%s/00000000.msg", as->name, mf->Parent, mf->Name);
	}
	else
	{
		pos = strlen(as->name) + strlen(mf->Name) + 2;
		sprintf(namebuf, "%s/%s/00000000.msg", as->name, mf->Name);
	}

	makename(&namebuf[pos], mi->Id);

	if(!stat(namebuf, &bleah) && (tmp = fopen(namebuf, FOPEN_READ_BINARY)))
	{
		mailbuf = calloc(1, bleah.st_size+1);
		if(mailbuf && !fread(mailbuf, bleah.st_size, 1, tmp))
		{
			free(mailbuf);
			mailbuf = NULL;
		}
		fclose(tmp);
		*len = bleah.st_size;
	}
	free(namebuf);
	dw_mutex_unlock(backend_mtx);
	return mailbuf;
}

/* Create a raw mail entry associated with a mail item */
int API backend_newmail(Account *acc, MailFolder *mf, MailItem *mi, char *buf, unsigned long len)
{
	AccountStruct *as = (AccountStruct *)acc;
	char *namebuf;
	FILE *tmp;
	int pos;

	if(!as || !mf || !mi)
		return 1;

	dw_mutex_lock(backend_mtx);
	namebuf = malloc(strlen(as->name) + strlen(mf->Name) + strlen(mf->Parent) + 16);
	if(mf->Parent[0])
	{
		pos = strlen(as->name) + strlen(mf->Name) + strlen(mf->Parent) + 3;
		sprintf(namebuf, "%s/%s/%s/00000000.msg", as->name, mf->Parent, mf->Name);
	}
	else
	{
		pos = strlen(as->name) + strlen(mf->Name) + 2;
		sprintf(namebuf, "%s/%s/00000000.msg", as->name, mf->Name);
	}

	makename(&namebuf[pos], mi->Id);

	if((tmp = fopen(namebuf, FOPEN_WRITE_BINARY)) != NULL)
	{
		fwrite(buf, len, 1, tmp);
		fclose(tmp);
	}
	free(namebuf);
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

