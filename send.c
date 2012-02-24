/*
 * This file deals with getting messages from
 * the remote email server.
 */
#include <string.h>
#include <dwcompat.h>
#include <dw.h>
#include "backend.h"
#include "dmail.h"
#include "parse.h"

extern BackendPlugin plugin_list[];
extern HWND stext3;

char *find_address(char *from)
{
	int start, len = strlen(from);

	/* Find the from domain */
	for(start=0;start<len;start++)
	{
		if(from[start] == '<')
		{
			start++;
			break;
		}
	}
	from = strdup(from[start] ? &from[start] : from);
	len = strlen(from);
	for(start=0;start<len;start++)
	{
		if(from[start] == '>')
			from[start] = 0;
	}
	return from;
}

char *find_domain(char *from)
{
	int atsym, len = strlen(from);

	/* Find the from domain */
	for(atsym=0;atsym<len;atsym++)
	{
		if(from[atsym] == '@')
		{
			atsym++;
			break;
		}
	}
	from = strdup(from[atsym] ? &from[atsym] : from);
	len = strlen(from);
	for(atsym=0;atsym<len;atsym++)
	{
		if(from[atsym] == '>')
			from[atsym] = 0;
	}
	return from;
}

typedef struct _smtpdata {
	int messages;
	int current;
	int status;
	int sentlen;
	unsigned long datalen;
	char *data;
	MailFolder *sentmail;
} SMTPData;

int SMTPIteration(AccountInfo *ai, MailFolder *mf, int controlfd, MailItem *mi, SMTPData *smd)
{
	char controlbuffer[1025] = "";
	static char nexttime[513] = "";
	int z, gah, start, amnt;

	start = 0;
	strncpy(controlbuffer, nexttime, 512);
	amnt = sockread(controlfd, &controlbuffer[strlen(nexttime)], 512, 0);
	if(amnt < 1)
	{
		dw_window_set_text(stext3, "Unexpected disconnect.");
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

				if(strlen(&controlbuffer[start]) > 0)
				{
					if(smd->status == -1 && strncmp(mystart, "220", 3) == 0)
					{
						/* We got the welcome message, be polite, say "helo" */
						char *from;

						dw_window_set_text(stext3, "Hello...");

						from = find_domain(mi[smd->current].From);
						socksprint(controlfd, vargs(alloca(1024), 1023, "HELO %s\r\n", from));
						free(from);
						smd->status++;
					}
					else if(smd->status == 0 && strncmp(mystart, "250", 3) == 0)
					{
						char *from;

						/* Done saying hello, let's get a message to send */
						dw_window_set_text(stext3, "From...");

						from = find_address(mi[smd->current].From);
						socksprint(controlfd, vargs(alloca(1024), 1023, "MAIL FROM:<%s>\r\n", from));
						free(from);
						smd->status++;
					}
					else if(smd->status == 1 && strncmp(mystart, "250", 3) == 0)
					{
						MailParsed *mp = malloc(sizeof(MailParsed));

						smd->data = plugin_list[ai->Plug].getmail(mi[smd->current].Acc, mi[smd->current].Folder, mi, &smd->datalen);
						parse_message(smd->data, smd->datalen, mp);

						/* Our from host was accepted, send recipients */
						dw_window_set_text(stext3, "Recipients...");

						if(!mp->to[0])
						{
							char *to = find_address(mi[smd->current].To);
							socksprint(controlfd, vargs(alloca(1024), 1023, "RCPT TO:<%s>\r\n", to));
							free(to);
						}
						else
						{
							int n = 0;

							while(mp->to[n] && n < MAX_TO_ADDRESSES)
							{
								char *to = find_address(mp->to[n]);
								socksprint(controlfd, vargs(alloca(1024), 1023, "RCPT TO:<%s>\r\n", to));
								free(to);
								n++;
							}
						}
						free(mp);
						smd->status++;
					}
					else if(smd->status == 2 && strncmp(mystart, "250", 3) == 0)
					{
						/* Our recipients were accepted, send message */
						char textbuf[100];

						sprintf(textbuf, "Sending message %d of %d...", smd->current+1, smd->messages);
						dw_window_set_text(stext3, textbuf);
						socksprint(controlfd, "DATA\r\n");
						smd->status++;
					}
					else if(smd->status == 3 && strncmp(mystart, "250", 3) == 0)
					{
						/* Do nothing.. probably resulting from multiple
						 * recipients. ;)
						 */
					}
					else if(smd->status == 3 && strncmp(mystart, "354", 3) == 0)
					{
						smd->sentlen = 0;
						smd->status++;
					}
					else if(smd->status == 4 && strncmp(mystart, "250", 3) == 0)
					{
						/* Move messages to sent mail if found */
						if(smd->sentmail)
						{
							MailItem mailitem;

							memcpy(&mailitem, &mi[smd->current], sizeof(MailItem));
							plugin_list[ai->Plug].newitem(ai->Acc, smd->sentmail, &mailitem);
							plugin_list[ai->Plug].newmail(ai->Acc, smd->sentmail, &mailitem, smd->data, smd->datalen);
						}

						/* Free the buffer and then delete the message from
						 * the outbox.
						 */
						plugin_list[ai->Plug].free(mi[smd->current].Acc, smd->data);
						plugin_list[ai->Plug].delitem(mi[smd->current].Acc, mf, &mi[smd->current]);
						smd->data = NULL;
						smd->datalen = 0;
						smd->current++;
						smd->status = 0;
						if(smd->current >= smd->messages)
						{
							char textbuf[100];

							sprintf(textbuf, "%d message%s sent.", smd->messages, smd->messages == 1 ? "" : "s");
							dw_window_set_text(stext3, textbuf);
							socksprint(controlfd, "QUIT\r\n");
							sockclose(controlfd);
							controlfd = 0;
						}
						else
						{
							char *from;

							/* Done saying hello, let's get a message to send */
							dw_window_set_text(stext3, "From...");

							from = find_address(mi[smd->current].From);
							socksprint(controlfd, vargs(alloca(1024), 1023, "MAIL FROM:<%s>\r\n", from));
							free(from);
							smd->status++;
						}
					}
					else
					{
						/* An unhandled error occurred */
						dw_window_set_text(stext3, "Error sending messages.");
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

void HandleSMTPSend(AccountInfo *ai, MailFolder *mf, int controlfd)
{
	fd_set readset, writeset;
	struct timeval tv = { 5, 0 }, slowtv = { 5, 0 };
	MailItem *mi = plugin_list[ai->Plug].getitems(mf->Acc, mf);
	SMTPData smd = {0, 0, -1, 0, 0, NULL, NULL };
	int n = 0;

	/* Find a mailbox to put sent messages */
	while(ai->Folders[n].Name[0])
	{
		if(strcmp(ai->Folders[n].Name, "Sent Mail") == 0)
		{
			smd.sentmail = &ai->Folders[n];
			break;
		}
		n++;
	}

	/* Count how many messages we have to send */
	while(mi[smd.messages].Id)
	{
		smd.messages++;
	}

	FD_ZERO(&readset);
	FD_ZERO(&writeset);
	FD_SET(controlfd, &readset);

	while(mi && mi[smd.current].Id && controlfd && select(controlfd + 1, &readset, NULL, NULL, &tv) > -1)
	{
		int amnt;

		if(smd.data && smd.datalen && smd.status == 4 && controlfd && FD_ISSET(controlfd, &writeset))
		{
			int tosend = smd.datalen - smd.sentlen;

			if(tosend)
			{
				/* Send data as we can. */
				amnt = sockwrite(controlfd, &smd.data[smd.sentlen], tosend > 512 ? 512 : tosend, 0);
				if(amnt > 0)
					smd.sentlen += amnt;
			}
			else
			{
				/* Finish the email with <CRLF>.<CRLF> */
				socksprint(controlfd, "\r\n.\r\n");
			}

		}
		if(controlfd && FD_ISSET(controlfd, &readset))
			controlfd = SMTPIteration(ai, mf, controlfd, mi, &smd);

		FD_ZERO(&readset);
		FD_ZERO(&writeset);
		FD_SET(controlfd, &readset);
		if(smd.data && smd.datalen)
			FD_SET(controlfd, &writeset);

		memcpy(&tv, &slowtv, sizeof(struct timeval));
	}
	if(controlfd)
	{
		dw_window_set_text(stext3, "Unexpected disconnect.");
		sockclose(controlfd);
		controlfd = 0;
	}
}

void API Mail_Send_Thread(void *data)
{
	AccountInfo *ai = (AccountInfo *)data;
	MailFolder *mf = NULL;

	if(!ai || !ai->Folders)
	{
		dw_messagebox("DynamicMail", DW_MB_OK | DW_MB_ERROR, "Send Mail Thread failed to initialize!");
		return;
	}
	else
	{
		int n = 0;

		while(ai->Folders[n].Name[0])
		{
			if(strcmp(ai->Folders[n].Name, "Outbox") == 0)
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
		if(dw_event_wait(ai->SendEve, 1000) == DW_ERROR_NONE)
		{
			/* Send mail! */
			struct hostent *hostnm;
			struct sockaddr_in server;
			int controlfd = 0, ipaddr = 0;
			AccountSettings *set = &ai->Settings;

			dw_event_reset(ai->SendEve);

			dw_window_set_text(stext3, "Connecting...");

			if(isip(set->SendHostName))
				ipaddr = inet_addr(set->RecvHostName);
			else
			{
				hostnm = gethostbyname(set->SendHostName);
				if(!hostnm)
					dw_window_set_text(stext3, "Host lookup failed.");
				else
					ipaddr = *((unsigned long *)hostnm->h_addr);
			}

			if(ipaddr && ipaddr != -1)
			{
				server.sin_family      = AF_INET;
				server.sin_port        = htons(set->SendHostPort);
				server.sin_addr.s_addr = ipaddr;
				if((controlfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 || connect(controlfd, (struct sockaddr *)&server, sizeof(server)))
				{
					dw_window_set_text(stext3, "Could not connect.");
					controlfd = 0;
				} else
				{
					/* We are connected, wait for the welcome */
					dw_window_set_text(stext3, "Connected.");
					nonblock(controlfd);
				}
				if(controlfd)
				{
					switch(set->SendHostType)
					{
					case HOST_TYPE_SMTP:
						HandleSMTPSend(ai, mf, controlfd);
						break;
					default:
						dw_window_set_text(stext3, "Unhandled protocol.");
						sockclose(controlfd);
					}
				}
			}
		}
	}
}
