/*
 * This file deals with getting messages from
 * the remote email server.
 */

#include <string.h>
#include <compat.h>
#include <dw.h>
#include "backend.h"
#include "dmail.h"
#include "parse.h"
#include "datetime.h"

extern BackendPlugin plugin_list[];
extern HWND stext2;

typedef struct _pop3data {
	int messages;
	int current;
	int bytes;
	int status;
	int datalen;
	char *data;
	MailItem mi;
} POP3Data;

int POP3Iteration(AccountInfo *ai, MailFolder *mf, int controlfd, AccountSettings *set, POP3Data *p3d)
{
	char controlbuffer[1025] = "";
	static char nexttime[513] = "";
	int z, gah, start, amnt;

	start = 0;
	strncpy(controlbuffer, nexttime, 512);
	amnt = sockread(controlfd, &controlbuffer[strlen(nexttime)], 512, 0);
	if(amnt < 1)
	{
		dw_window_set_text(stext2, "Unexpected disconnect.");
		sockclose(controlfd);
		controlfd = 0;
	}
	else
	{
		controlbuffer[amnt+strlen(nexttime)] = 0;
		nexttime[0] = 0;
		gah = strlen(controlbuffer);
		for(z=0;z<gah;z++)
		{
			if(controlbuffer[z] == '\r' || controlbuffer[z] == '\n')
			{
				char *mystart = &controlbuffer[start];

				if(controlbuffer[z] == '\r' && controlbuffer[z+1] == '\n')
				{
					controlbuffer[z] = 0;
					controlbuffer[z+1] = 0;
					z++;
				}
				else
					controlbuffer[z] = 0;

				if(p3d->status == 5)
				{
					if(strncmp(mystart, ".", 2) == 0)
					{
						char textbuf[100];

						if(p3d->data && p3d->datalen)
						{
							MailParsed mp;

							parse_message(p3d->data, p3d->datalen, &mp);

							/* Copy the items */
							if(mp.to[0])
								strncpy(p3d->mi.To, mp.to[0], MAIL_ITEM_MAX);
							if(mp.from)
								strncpy(p3d->mi.From, mp.from, MAIL_ITEM_MAX);
							if(mp.topic)
								strncpy(p3d->mi.Topic, mp.topic, MAIL_ITEM_MAX);

							p3d->mi.Date.day = findday(mp.date);
							p3d->mi.Date.month = findmonth(mp.date);
							p3d->mi.Date.year = findyear(mp.date);
							p3d->mi.Time.hours = findhour(mp.date);
							p3d->mi.Time.minutes = findmin(mp.date);
							p3d->mi.Time.seconds = findsec(mp.date);

							parse_free(&mp);

							p3d->mi.Size = p3d->datalen;

							/* Yay! we have a whole message.. lets save it! */
							plugin_list[ai->Plug].newitem(ai->Acc, mf, &p3d->mi);
							plugin_list[ai->Plug].newmail(ai->Acc, mf, &p3d->mi, p3d->data, p3d->datalen);

							free(p3d->data);
						}

						/* Reset the message state variables */
						p3d->data = NULL;
						p3d->datalen = 0;
						memset(&p3d->mi, 0, sizeof(MailItem));

						sprintf(textbuf, "Deleting message %d...", p3d->current);
						dw_window_set_text(stext2, textbuf);
						socksprint(controlfd, vargs(alloca(1024), 1023, "DELE %d\r\n", p3d->current));

						p3d->status++;
					}
					else
					{
						/* More message data... save it for later */
						int len = strlen(mystart);
						char *tmp = malloc(len + p3d->datalen + 3);

						if(p3d->data)
							memcpy(tmp, p3d->data, p3d->datalen);
						if(len)
							memcpy(&tmp[p3d->datalen], mystart, len);
						p3d->datalen += len + 2;
						tmp[p3d->datalen-2] = '\r';
						tmp[p3d->datalen-1] = '\n';
						tmp[p3d->datalen] = 0;
						if(p3d->data)
							free(p3d->data);
						p3d->data = tmp;
					}
				}
				else if(strlen(&controlbuffer[start]) > 0)
				{
					if(p3d->status == -1 && strncmp(mystart, "+OK", 3) == 0)
					{
						/* We got the welcome message, let's login */
						dw_window_set_text(stext2, "Username...");
						socksprint(controlfd, vargs(alloca(1024), 1023, "USER %s\r\n", set->RecvHostUser));
						p3d->status++;
					}
					else if(p3d->status == 0 && strncmp(mystart, "+OK", 3) == 0)
					{
						/* Our username was accepted, send password */
						dw_window_set_text(stext2, "Password...");
						socksprint(controlfd, vargs(alloca(1024), 1023, "PASS %s\r\n", set->RecvHostPass));
						p3d->status++;
					}
					else if((p3d->status == 0 || p3d->status == 1) && strncmp(mystart, "-ERR", 3) == 0)
					{
						/* Either the user or pass request failed, abort. */
						dw_window_set_text(stext2, "Login failed.");
						socksprint(controlfd, "QUIT\r\n");
						sockclose(controlfd);
						controlfd = 0;
					}
					else if(p3d->status == 1 && strncmp(mystart, "+OK", 3) == 0)
					{
						/* Our login was successful! Get a list of messages */
						dw_window_set_text(stext2, "Status...");
						socksprint(controlfd, "STAT\r\n");
						p3d->status++;
					}
					else if(p3d->status == 2 && strncmp(mystart, "+OK", 3) == 0)
					{
						char textbuf[100];

						p3d->messages = atoi(&mystart[3]);
						if(p3d->messages)
						{
							/* Our login was successful! Get a list of messages */
							sprintf(textbuf, "Getting message %d of %d...", p3d->current, p3d->messages);
							dw_window_set_text(stext2, textbuf);
							socksprint(controlfd, vargs(alloca(1024), 1023, "LIST %d\r\n", p3d->current));
							p3d->status++;
						}
						else
						{
							dw_window_set_text(stext2, "No new mail.");
							socksprint(controlfd, "QUIT\r\n");
							sockclose(controlfd);
							controlfd = 0;
						}
					}
					else if(p3d->status == 3 && strncmp(mystart, "+OK", 3) == 0)
					{
						char textbuf[100];

						/* We have a size for our message...
						 * request the message contents!
						 */
						sprintf(textbuf, "+OK %d ", p3d->current);
						p3d->bytes = atoi(&mystart[strlen(textbuf)]);
						socksprint(controlfd, vargs(alloca(1024), 1023, "RETR %d\r\n", p3d->current));
						p3d->status++;
					}
					else if(p3d->status == 4 && strncmp(mystart, "+OK", 3) == 0)
					{
						/* Just proceed to the next step */
						p3d->status++;
					}
					else if(p3d->status == 6 && strncmp(mystart, "+OK", 3) == 0)
					{
						char textbuf[100];

						/* Move on to the next message */
						p3d->current++;
						if(p3d->current > p3d->messages)
						{
							/* Success! We have retrieved our new messages */
							sprintf(textbuf, "%d new message%s.", p3d->messages, p3d->messages == 1 ? "" : "s");
							dw_window_set_text(stext2, textbuf);
							socksprint(controlfd, "QUIT\r\n");
							sockclose(controlfd);
							controlfd = 0;
						}
						else
						{
							sprintf(textbuf, "Getting message %d of %d...", p3d->current, p3d->messages);
							dw_window_set_text(stext2, textbuf);
							socksprint(controlfd, vargs(alloca(1024), 1023, "LIST %d\r\n", p3d->current));
							p3d->status = 3;
						}
					}
					else if(strncmp(mystart, "-ERR", 3) == 0)
					{
						/* An unhandled error occurred */
						dw_window_set_text(stext2, "Error getting messages.");
						socksprint(controlfd, "QUIT\r\n");
						sockclose(controlfd);
						controlfd = 0;
						dw_messagebox("DynamicMail Error", DW_MB_OK | DW_MB_ERROR, mystart);
					}
				}
				start = z+1;
			}
		}
		if(strlen(&controlbuffer[start]) > 0)
			strncpy(nexttime, &controlbuffer[start], 512);
		else
			nexttime[0] = 0;
	}
	return controlfd;
}

void HandlePOP3(AccountInfo *ai, MailFolder *mf, int controlfd)
{
	fd_set readset;
	struct timeval tv = { 5, 0 }, slowtv = { 5, 0 };
	POP3Data p3d = {0, 1, 0, -1, 0, NULL };

	memset(&p3d.mi, 0, sizeof(MailItem));

	FD_ZERO(&readset);
	FD_SET(controlfd, &readset);

	while(controlfd && select(controlfd + 1, &readset, NULL, NULL, &tv) > -1)
	{
		if(controlfd && FD_ISSET(controlfd, &readset))
			controlfd = POP3Iteration(ai, mf, controlfd, &ai->Settings, &p3d);

		FD_ZERO(&readset);
		FD_SET(controlfd, &readset);

		memcpy(&tv, &slowtv, sizeof(struct timeval));
	}
	if(controlfd)
	{
		dw_window_set_text(stext2, "Unexpected disconnect.");
		sockclose(controlfd);
		controlfd = 0;
	}
}

void API Mail_Receive_Thread(void *data)
{
	AccountInfo *ai = (AccountInfo *)data;
	MailFolder *mf = NULL;

	if(!ai || !ai->Folders)
	{
		dw_messagebox("DynamicMail", DW_MB_OK | DW_MB_ERROR, "Receive Mail Thread failed to initialize!");
		return;
	}
	else
	{
		int n = 0;

		while(ai->Folders[n].Name[0])
		{
			if(strcmp(ai->Folders[n].Name, "Inbox") == 0)
			{
				mf = &ai->Folders[n];
				break;
			}
			n++;
		}
	}

	if(!mf)
		return;

	while(ai->Acc)
	{
		if(dw_event_wait(ai->RecvEve, 1000) == DW_ERROR_NONE)
		{
			/* Check mail! */
    		struct hostent *hostnm;
			struct sockaddr_in server;
			int controlfd = 0, ipaddr = 0;
			AccountSettings *set = &ai->Settings;

			dw_event_reset(ai->RecvEve);

			dw_window_set_text(stext2, "Connecting...");

			if(isip(set->RecvHostName))
				ipaddr = inet_addr(set->RecvHostName);
			else
			{
				hostnm = gethostbyname(set->RecvHostName);
				if(!hostnm)
					dw_window_set_text(stext2, "Host lookup failed.");
				else
					ipaddr = *((unsigned long *)hostnm->h_addr);
			}

			if(ipaddr && ipaddr != -1)
			{
				server.sin_family      = AF_INET;
				server.sin_port        = htons(set->RecvHostPort);
				server.sin_addr.s_addr = ipaddr;
				if((controlfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 || connect(controlfd, (struct sockaddr *)&server, sizeof(server)))
				{
					dw_window_set_text(stext2, "Could not connect.");
					controlfd = 0;
				} else
				{
					/* We are connected, wait for the welcome */
					dw_window_set_text(stext2, "Connected.");
					nonblock(controlfd);
				}
				if(controlfd)
				{
					switch(set->RecvHostType)
					{
					case HOST_TYPE_POP3:
						HandlePOP3(ai, mf, controlfd);
						break;
					default:
						dw_window_set_text(stext2, "Unhandled protocol.");
						sockclose(controlfd);
					}
				}
			}
		}
	}
}
