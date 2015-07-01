bool JSONfind(const char *s, const char *k, const char **r, size_t *l)
{
	const char *t = strstr(s, k);
	if (!t) return false;

	*r = NULL, *l = 0;
	t += strlen(k)+1;
	while (*t)
	{
		if (*t == '"')
		{
			if (!*r) *r = t+1;
			else
			{
				*l = t-*r;
				break;
			}
		}
		t++;
	}
	return (*r&&*l);
}

void url_encode(char *dest, const uchar *src)
{
	while (*src)
	{
		uchar c = (*src);

		if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') *(dest++) = c;
		else
		{
			*dest = '%';
			itoa(c, dest+1, 16);
			dest += 3;
		}

		src++;
	}
	*dest = '\0';
}

void text_decode(char *dest, const uchar *src)
{
	uchar c;
	while (c = (*src))
	{
		if (c != '\\') *(dest++) = c;
		else
		{
			char st[5];
			strncpy(st, (const char *)(src+2), 4);
			st[4] = '\0';
			*(dest++) = uni2cube((int)strtol(st, NULL, 16));
			src += 5;
		}
		src++;
	}
	*dest = '\0';
}

CURLM *curlMultiHandle = NULL;

class CChunk
{
public:
	char *msg, *name, *callback, *fromlang, *tolang;
	size_t len, cap;
	CURL *curlh;
	int type,
		con; // one of TransServices below

	CChunk() { name = NULL; con = 0; callback = NULL; fromlang = NULL; tolang = NULL; };
	int write(const char *s, size_t l)
	{
		if (len+l+1 > cap)
		{
			char *buf = new char[cap*2];
			memcpy(buf, msg, len);
			delete[] msg;
			msg = buf;
		}
		memcpy(&msg[len], s, l);
		len += l;
		return l;
	}
	char *getstr()
	{
		msg[len] = '\0';
		return msg;
	}
	void setName(const char *newname)
	{
		name = newstring(newname);
	}
	void setCB(const char *cb)
	{
		callback = newstring(cb);
	}
	void setCon(int c)
	{
		con = c;
	}
	~CChunk() { delete[] msg; if (name) delete[] name; if (callback) delete[] callback; if (fromlang) delete[] fromlang; if (tolang) delete[] tolang; }

	virtual char *getTranslation() { return NULL; };
	virtual char *getFromLang() { return NULL; };
};
vector<CChunk*> curlTrans;

size_t downloadWriteData(char *ptr, size_t size, size_t nmemb, void *userdata)
{
	CChunk *writeData = (CChunk*)userdata;
	return writeData->write(ptr, size*nmemb);;
}

enum TransServices
{
	TRANS_YANDEX = 0,		// GET https://translate.yandex.net/api/v1.5/tr.json/translate?key=[apikey]&lang=[tolang]&text=[text] ->JSON("text") https://tech.yandex.com/keys/get/?service=trnsl
	TRANS_MYMEMORY,			// GET http://api.mymemory.translated.net/get?q=[text]&langpair=[fromlang]|[tolang]&de=[email] ->JSON("translatedText")
};

SVARHSC(trans_to, "en");

SVARHSC(trans_yandex_apikey, "");

class CYandex: public CChunk
{
public:

	CYandex(CURL *handle, const char *from, const char *to, const char *text)
	{
		len = 0;
		curlh = handle;
		cap = 1000;
		msg = new char[1000];
		tolang = newstring(to? to: trans_to);

		type = TRANS_YANDEX;

		char url[5000], escaped[5000];
		uchar ubuf[5000];
		int numu = encodeutf8(ubuf, sizeof(ubuf)-1, (uchar *)text, strlen(text), NULL);
		ubuf[numu] = '\0';
		url_encode(escaped, ubuf);

		formatstring(url)("https://translate.yandex.net/api/v1.5/tr.json/translate?key=%s&lang=%s%s%s&text=%s", trans_yandex_apikey, from? from: "", from? "-": "", tolang, escaped);

		curl_easy_setopt(curlh, CURLOPT_URL, url);
		curl_easy_setopt(curlh, CURLOPT_FOLLOWLOCATION, 1);
		curl_easy_setopt(curlh, CURLOPT_WRITEFUNCTION, downloadWriteData);
		curl_easy_setopt(curlh, CURLOPT_WRITEDATA, this);
		curl_easy_setopt(curlh, CURLOPT_TIMEOUT, 120);
	}
	char *getTranslation()
	{
		static char ubuf[5000];
		int numu = decodeutf8((uchar *)ubuf, sizeof(ubuf)-1, (uchar *)getstr(), len, NULL);
		ubuf[numu] = '\0';

		char *tr;
		size_t ln;
		if (JSONfind((char *)ubuf, "\"text\"", (const char **)&tr, &ln))
		{
			strncpy(ubuf, tr, ln);
			ubuf[ln] = '\0';
			return ubuf;
		}
		return NULL;
	}
	char *getFromLang()
	{
		static char ubuf[5000];
		int numu = decodeutf8((uchar *)ubuf, sizeof(ubuf)-1, (uchar *)getstr(), len, NULL);
		ubuf[numu] = '\0';

		char *tr;
		size_t ln;
		if (JSONfind((char *)ubuf, "\"lang\"", (const char **)&tr, &ln))
		{
			strncpy(ubuf, tr, 2);
			ubuf[2] = '\0';
			return ubuf;
		}
		return NULL;
	}
};

SVARHSC(trans_mymemory_email, "");

class CMyMemory: public CChunk
{
public:
	CMyMemory(CURL *handle, const char *from, const char *to, const char *text)
	{
		len = 0;
		curlh = handle;
		cap = 1000;
		msg = new char[1000];
		fromlang = newstring(from);
		tolang = newstring(to? to: trans_to);

		type = TRANS_MYMEMORY;

		char url[5000], escaped[5000];
		uchar ubuf[5000];
		int numu = encodeutf8(ubuf, sizeof(ubuf)-1, (uchar *)text, strlen(text), NULL);
		ubuf[numu] = '\0';
		url_encode(escaped, ubuf);

		bool withemail = (trans_mymemory_email && trans_mymemory_email[0]);
		formatstring(url)("http://api.mymemory.translated.net/get?q=%s&langpair=%s|%s%s%s", escaped, from, tolang, withemail? "&de=": "", withemail? trans_mymemory_email: "");

		curl_easy_setopt(curlh, CURLOPT_URL, url);
		curl_easy_setopt(curlh, CURLOPT_FOLLOWLOCATION, 1);
		curl_easy_setopt(curlh, CURLOPT_WRITEFUNCTION, downloadWriteData);
		curl_easy_setopt(curlh, CURLOPT_WRITEDATA, this);
		curl_easy_setopt(curlh, CURLOPT_TIMEOUT, 120);
	}
	char *getTranslation()
	{
		static char ubuf[5000], obuf[5000];
		int numu = decodeutf8((uchar *)ubuf, sizeof(ubuf)-1, (uchar *)getstr(), len, NULL);
		ubuf[numu] = '\0';

		char *tr;
		size_t ln;
		if (JSONfind((char *)ubuf, "\"translatedText\"", (const char **)&tr, &ln))
		{
			strncpy(ubuf, tr, ln);
			ubuf[ln] = '\0';
			text_decode(obuf, (const uchar *)ubuf);
			return obuf;
		}
		return NULL;
	}
	char *getFromLang()
	{
		return fromlang;
	}
};

char *trans_lation = svariable("trans_lation", "", &trans_lation, NULL, IDF_READONLY);
char *trans_from = svariable("trans_from", "", &trans_from, NULL, IDF_READONLY);

int curlLastTry = 0;

void translateProcess()
{
	if (totalmillis-curlLastTry > 100 && curlTrans.length())
	{
		curlLastTry = totalmillis;

		int rh;
		curl_multi_perform(curlMultiHandle, &rh);

		if (rh)
		{
			fd_set fdread, fdwrite, fdexcep;
			int maxfd = -1;

			FD_ZERO(&fdread);
			FD_ZERO(&fdwrite);
			FD_ZERO(&fdexcep);

			if (curl_multi_fdset(curlMultiHandle, &fdread, &fdwrite, &fdexcep, &maxfd)) return;

			if (maxfd == -1) return;
			else
			{
				static timeval tv = {0, 0};
				select(maxfd+1, &fdread, &fdwrite, &fdexcep, &tv);
			}
		}

		CURLMsg *msg;
		int msgs_left;
		while (msg = curl_multi_info_read(curlMultiHandle, &msgs_left)) {
			if (msg->msg == CURLMSG_DONE) {
				CURL *e = msg->easy_handle;
				for (int i = 0; i < curlTrans.length(); i++)
				{
					if (curlTrans[i]->curlh == e)
					{
						CChunk *ct = curlTrans[i];
						if (ct->callback)
						{
							trans_lation = newstring(ct->getTranslation());
							trans_from = newstring(ct->getFromLang());
							execute(ct->callback);
							delete[] trans_lation;
							delete[] trans_from;
							trans_lation = trans_from = "";
						}
						else
						{
							const char *fromlang = ct->getFromLang();
							conoutf(ct->con? ct->con: CON_ECHO, "\fs\f3%s%s\f2[%s->%s]: \fr%s", ct->name? ct->name: "", ct->name? " ": "", fromlang? fromlang: "?", ct->tolang? ct->tolang: trans_to, ct->getTranslation());
						}

						delete curlTrans[i];
						curlTrans.remove(i);
					}
				}
				curl_multi_remove_handle(curlMultiHandle, e);
				curl_easy_cleanup(e);
			}
		}
	}
}

CChunk *addTrans(const char *s, const char *from = NULL, const char *to = NULL)
{
	if (curlMultiHandle == NULL) curlMultiHandle = curl_multi_init();
		 
	CURL *handle = curl_easy_init();
 	if(!handle) return NULL;

	if (from && !from[0]) from = NULL;

	CChunk *writeData;
	if (trans_yandex_apikey[0]) writeData = new CYandex(handle, from, to, s);
	else if (from) writeData = new CMyMemory(handle, from, to, s);
	else
	{
		conoutf("\f3could not find a compatible translation engine. please read documentation");
		return NULL;
	}

	CURLMcode ret = curl_multi_add_handle(curlMultiHandle, handle);
	if (ret != 0)
	{
		delete writeData;
		return NULL;
	}
	else
	{
		curlTrans.add(writeData);
		return writeData;
	}
}

void translatechat(int line)
{
	const char *cl = getchatconline(line);
	if (!cl) return;

	const char *del = strstr(cl, ":\f0 ");
	if (!del) del = strstr(cl, ":\f1 ");

	CChunk *tr = addTrans(del? del+4: cl);
	if (tr && del) tr->name = newstring(cl, del-cl);
}

ICOMMANDS("trans", "s", (char *s), { addTrans(s); });
ICOMMANDS("trans_", "ss", (char *from, char *s), { addTrans(s, from); });
ICOMMANDS("trans_chat", "i", (int *num), { translatechat(num? *num: 1); });
ICOMMANDS("trans_late", "ssss", (char *from, char *to, char *s, char *cb), { CChunk *h = addTrans(s, from, to); if (h) h->setCB(cb); });
ICOMMANDS("trans_say", "sss", (char *from, char *to, char *s), { CChunk *h = addTrans(s, from, to); if (h) h->setCon(CON_CHAT); });
ICOMMANDS("trans_sayteam", "sss", (char *from, char *to, char *s), { CChunk *h = addTrans(s, from, to); if (h) h->setCon(CON_TEAMCHAT); });
