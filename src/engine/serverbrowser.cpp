// serverbrowser.cpp: eihrul's concurrent resolver, and server browser window management

#include "engine.h"
#include "SDL_thread.h"

struct resolverthread
{
    SDL_Thread *thread;
    const char *query;
    int starttime;
};

struct resolverresult
{
    const char *query;
    ENetAddress address;
};

vector<resolverthread> resolverthreads;
vector<const char *> resolverqueries;
vector<resolverresult> resolverresults;
SDL_mutex *resolvermutex;
SDL_cond *querycond, *resultcond;

#define RESOLVERTHREADS 2
#define RESOLVERLIMIT 3000

int resolverloop(void * data)
{
    resolverthread *rt = (resolverthread *)data;
    SDL_LockMutex(resolvermutex);
    SDL_Thread *thread = rt->thread;
    SDL_UnlockMutex(resolvermutex);
    if(!thread || SDL_GetThreadID(thread) != SDL_ThreadID())
        return 0;
    while(thread == rt->thread)
    {
        SDL_LockMutex(resolvermutex);
        while(resolverqueries.empty()) SDL_CondWait(querycond, resolvermutex);
        rt->query = resolverqueries.pop();
        rt->starttime = totalmillis;
        SDL_UnlockMutex(resolvermutex);

        ENetAddress address = { ENET_HOST_ANY, ENET_PORT_ANY };
        enet_address_set_host(&address, rt->query);

        SDL_LockMutex(resolvermutex);
        if(rt->query && thread == rt->thread)
        {
            resolverresult &rr = resolverresults.add();
            rr.query = rt->query;
            rr.address = address;
            rt->query = NULL;
            rt->starttime = 0;
            SDL_CondSignal(resultcond);
        }
        SDL_UnlockMutex(resolvermutex);
    }
    return 0;
}

void resolverinit()
{
    resolvermutex = SDL_CreateMutex();
    querycond = SDL_CreateCond();
    resultcond = SDL_CreateCond();

    SDL_LockMutex(resolvermutex);
    loopi(RESOLVERTHREADS)
    {
        resolverthread &rt = resolverthreads.add();
        rt.query = NULL;
        rt.starttime = 0;
        rt.thread = SDL_CreateThread(resolverloop, &rt);
    }
    SDL_UnlockMutex(resolvermutex);
}

void resolverstop(resolverthread &rt)
{
    SDL_LockMutex(resolvermutex);
    if(rt.query)
    {
#ifndef __APPLE__
        SDL_KillThread(rt.thread);
#endif
        rt.thread = SDL_CreateThread(resolverloop, &rt);
    }
    rt.query = NULL;
    rt.starttime = 0;
    SDL_UnlockMutex(resolvermutex);
} 

void resolverclear()
{
    if(resolverthreads.empty()) return;

    SDL_LockMutex(resolvermutex);
    resolverqueries.shrink(0);
    resolverresults.shrink(0);
    loopv(resolverthreads)
    {
        resolverthread &rt = resolverthreads[i];
        resolverstop(rt);
    }
    SDL_UnlockMutex(resolvermutex);
}

void resolverquery(const char *name)
{
    if(resolverthreads.empty()) resolverinit();

    SDL_LockMutex(resolvermutex);
    resolverqueries.add(name);
    SDL_CondSignal(querycond);
    SDL_UnlockMutex(resolvermutex);
}

bool resolvercheck(const char **name, ENetAddress *address)
{
    bool resolved = false;
    SDL_LockMutex(resolvermutex);
    if(!resolverresults.empty())
    {
        resolverresult &rr = resolverresults.pop();
        *name = rr.query;
        address->host = rr.address.host;
        resolved = true;
    }
    else loopv(resolverthreads)
    {
        resolverthread &rt = resolverthreads[i];
        if(rt.query && totalmillis - rt.starttime > RESOLVERLIMIT)        
        {
            resolverstop(rt);
            *name = rt.query;
            resolved = true;
        }    
    }
    SDL_UnlockMutex(resolvermutex);
    return resolved;
}

bool resolverwait(const char *name, ENetAddress *address)
{
    if(resolverthreads.empty()) resolverinit();

    defformatstring(text)("resolving %s... (esc to abort)", name);
    renderprogress(0, text);

    SDL_LockMutex(resolvermutex);
    resolverqueries.add(name);
    SDL_CondSignal(querycond);
    int starttime = SDL_GetTicks(), timeout = 0;
    bool resolved = false;
    for(;;) 
    {
        SDL_CondWaitTimeout(resultcond, resolvermutex, 250);
        loopv(resolverresults) if(resolverresults[i].query == name) 
        {
            address->host = resolverresults[i].address.host;
            resolverresults.remove(i);
            resolved = true;
            break;
        }
        if(resolved) break;
    
        timeout = SDL_GetTicks() - starttime;
        renderprogress(min(float(timeout)/RESOLVERLIMIT, 1.0f), text);
        if(interceptkey(SDLK_ESCAPE)) timeout = RESOLVERLIMIT + 1;
        if(timeout > RESOLVERLIMIT) break;    
    }
    if(!resolved && timeout > RESOLVERLIMIT)
    {
        loopv(resolverthreads)
        {
            resolverthread &rt = resolverthreads[i];
            if(rt.query == name) { resolverstop(rt); break; }
        }
    }
    SDL_UnlockMutex(resolvermutex);
    return resolved;
}

SDL_Thread *connthread = NULL;
SDL_mutex *connmutex = NULL;
SDL_cond *conncond = NULL;

struct connectdata
{
    ENetSocket sock;
    ENetAddress address;
    int result;
};

// do this in a thread to prevent timeouts
// could set timeouts on sockets, but this is more reliable and gives more control
int connectthread(void *data)
{
    SDL_LockMutex(connmutex);
    if(!connthread || SDL_GetThreadID(connthread) != SDL_ThreadID())
    {
        SDL_UnlockMutex(connmutex);
        return 0;
    }
    connectdata cd = *(connectdata *)data;
    SDL_UnlockMutex(connmutex);

    int result = enet_socket_connect(cd.sock, &cd.address);

    SDL_LockMutex(connmutex);
    if(!connthread || SDL_GetThreadID(connthread) != SDL_ThreadID())
    {
        enet_socket_destroy(cd.sock);
        SDL_UnlockMutex(connmutex);
        return 0;
    }
    ((connectdata *)data)->result = result;
    SDL_CondSignal(conncond);
    SDL_UnlockMutex(connmutex);

    return 0;
}

#define CONNLIMIT 20000

int connectwithtimeout(ENetSocket sock, const char *hostname, const ENetAddress &address)
{
    defformatstring(text)("connecting to %s... (esc to abort)", hostname);
    renderprogress(0, text);

    if(!connmutex) connmutex = SDL_CreateMutex();
    if(!conncond) conncond = SDL_CreateCond();
    SDL_LockMutex(connmutex);
    connectdata cd = { sock, address, -1 };
    connthread = SDL_CreateThread(connectthread, &cd);

    int starttime = SDL_GetTicks(), timeout = 0;
    for(;;)
    {
        if(!SDL_CondWaitTimeout(conncond, connmutex, 250))
        {
            if(cd.result<0) enet_socket_destroy(sock);
            break;
        }      
        timeout = SDL_GetTicks() - starttime;
        renderprogress(min(float(timeout)/CONNLIMIT, 1.0f), text);
        if(interceptkey(SDLK_ESCAPE)) timeout = CONNLIMIT + 1;
        if(timeout > CONNLIMIT) break;
    }

    /* thread will actually timeout eventually if its still trying to connect
     * so just leave it (and let it destroy socket) instead of causing problems on some platforms by killing it 
     */
    connthread = NULL;
    SDL_UnlockMutex(connmutex);

    return cd.result;
}

GeoIP *geoip = NULL;
int geoipdisabled = false;

enum { UNRESOLVED = 0, RESOLVING, RESOLVED };

struct serverinfo
{
    enum 
    { 
        WAITING = INT_MAX,

        MAXPINGS = 3 
    };

    string name, map, sdesc;
    int port, numplayers, resolved, ping, lastping, nextping;
    int pings[MAXPINGS];
    vector<int> attr;
    ENetAddress address;
    bool keep;
    const char *password, *country;

    serverinfo()
     : port(-1), numplayers(0), resolved(UNRESOLVED), keep(false), password(NULL), country(NULL)
    {
        name[0] = map[0] = sdesc[0] = '\0';
        clearpings();
    }

    ~serverinfo()
    {
        DELETEA(password);
    }

    void clearpings()
    {
        ping = WAITING;
        loopk(MAXPINGS) pings[k] = WAITING;
        nextping = 0;
        lastping = -1;
    }

    void cleanup()
    {
        clearpings();
        attr.setsize(0);
        numplayers = 0;
    }

    void reset()
    {
        lastping = -1;
    }

    void checkdecay(int decay)
    {
        if(lastping >= 0 && totalmillis - lastping >= decay)
            cleanup();
        if(lastping < 0) lastping = totalmillis;
    }

    void calcping()
    {
        int numpings = 0, totalpings = 0;
        loopk(MAXPINGS) if(pings[k] != WAITING) { totalpings += pings[k]; numpings++; }
        ping = numpings ? totalpings/numpings : WAITING;
    }

    void addping(int rtt, int millis)
    {
        if(millis >= lastping) lastping = -1;
        pings[nextping] = rtt;
        nextping = (nextping+1)%MAXPINGS;
        calcping();
    }

    static bool compare(serverinfo *a, serverinfo *b)
    {
        bool ac = server::servercompatible(a->name, a->sdesc, a->map, a->ping, a->attr, a->numplayers),
             bc = server::servercompatible(b->name, b->sdesc, b->map, b->ping, b->attr, b->numplayers);
        if(ac > bc) return true;
        if(bc > ac) return false;
        if(a->keep > b->keep) return true;
        if(a->keep < b->keep) return false;
        if(a->numplayers < b->numplayers) return false;
        if(a->numplayers > b->numplayers) return true;
        if(a->ping > b->ping) return false;
        if(a->ping < b->ping) return true;
        int cmp = strcmp(a->name, b->name);
        if(cmp != 0) return cmp < 0;
        if(a->port < b->port) return true;
        if(a->port > b->port) return false;
        return false;
    }
};

vector<serverinfo *> servers;
ENetSocket pingsock = ENET_SOCKET_NULL;
int lastinfo = 0;

static serverinfo *newserver(const char *name, int port, uint ip = ENET_HOST_ANY)
{
    serverinfo *si = new serverinfo;
    si->address.host = ip;
    si->address.port = server::serverinfoport(port);
    if(ip!=ENET_HOST_ANY) si->resolved = RESOLVED;

    si->port = port;
	int servip = enet_address_get_host_ip(&si->address, si->name, sizeof(si->name));
    if(name) copystring(si->name, name);
    else if(ip==ENET_HOST_ANY || servip < 0)
    {
        delete si;
        return NULL;

    }

    servers.add(si);

    return si;
}

void addserver(const char *name, int port, const char *password, bool keep)
{
    if(port <= 0) port = server::serverport();
    loopv(servers)
    {
        serverinfo *s = servers[i];
        if(strcmp(s->name, name) || s->port != port) continue;
        if(password && (!s->password || strcmp(s->password, password)))
        {
            DELETEA(s->password);
            s->password = newstring(password);
        }
        if(keep && !s->keep) s->keep = true;
        return;
    }
    serverinfo *s = newserver(name, port);
    if(!s) return;
    if(password) s->password = newstring(password);
    s->keep = keep;
}

VARP(searchlan, 0, 0, 1);
VARP(servpingrate, 1000, 5000, 60000);
VARP(servpingdecay, 1000, 15000, 60000);
VARP(maxservpings, 0, 10, 1000);

void pingservers()
{
    if(pingsock == ENET_SOCKET_NULL) 
    {
        pingsock = enet_socket_create(ENET_SOCKET_TYPE_DATAGRAM);
        if(pingsock == ENET_SOCKET_NULL)
        {
            lastinfo = totalmillis;
            return;
        }
        enet_socket_set_option(pingsock, ENET_SOCKOPT_NONBLOCK, 1);
        enet_socket_set_option(pingsock, ENET_SOCKOPT_BROADCAST, 1);
    }
    ENetBuffer buf;
    uchar ping[MAXTRANS];
    ucharbuf p(ping, sizeof(ping));
    putint(p, totalmillis);

    static int lastping = 0;
    if(lastping >= servers.length()) lastping = 0;
    loopi(maxservpings ? min(servers.length(), maxservpings) : servers.length())
    {
        serverinfo &si = *servers[lastping];
        if(++lastping >= servers.length()) lastping = 0;
        if(si.address.host == ENET_HOST_ANY) continue;
        buf.data = ping;
        buf.dataLength = p.length();
        enet_socket_send(pingsock, &si.address, &buf, 1);
        
        si.checkdecay(servpingdecay);
    }
    if(searchlan)
    {
        ENetAddress address;
        address.host = ENET_HOST_BROADCAST;
        address.port = server::laninfoport();
        buf.data = ping;
        buf.dataLength = p.length();
        enet_socket_send(pingsock, &address, &buf, 1);
    }
    lastinfo = totalmillis;
}
  
void checkresolver()
{
    int resolving = 0;
    loopv(servers)
    {
        serverinfo &si = *servers[i];
        if(si.resolved == RESOLVED) continue;
        if(si.address.host == ENET_HOST_ANY)
        {
            if(si.resolved == UNRESOLVED) { si.resolved = RESOLVING; resolverquery(si.name); }
            resolving++;
        }
    }
    if(!resolving) return;

    const char *name = NULL;
    for(;;)
    {
        ENetAddress addr = { ENET_HOST_ANY, ENET_PORT_ANY };
        if(!resolvercheck(&name, &addr)) break;
        loopv(servers)
        {
            serverinfo &si = *servers[i];
            if(name == si.name)
            {
                si.resolved = RESOLVED; 
                si.address.host = addr.host;
				if (!geoipdisabled) si.country = GeoIP_country_code_by_ipnum(geoip, endianswap(si.address.host));

                break;
            }
        }
    }
}

static int lastreset = 0;

void checkpings()
{
    if(pingsock==ENET_SOCKET_NULL) return;
    enet_uint32 events = ENET_SOCKET_WAIT_RECEIVE;
    ENetBuffer buf;
    ENetAddress addr;
    uchar ping[MAXTRANS];
    char text[MAXTRANS];
    buf.data = ping; 
    buf.dataLength = sizeof(ping);
    while(enet_socket_wait(pingsock, &events, 0) >= 0 && events)
    {
        int len = enet_socket_receive(pingsock, &addr, &buf, 1);
        if(len <= 0) return;  
        serverinfo *si = NULL;
        loopv(servers) if(addr.host == servers[i]->address.host && addr.port == servers[i]->address.port) { si = servers[i]; break; }
        if(!si && searchlan) si = newserver(NULL, server::serverport(addr.port), addr.host); 
        if(!si) continue;
        ucharbuf p(ping, len);
        int millis = getint(p), rtt = clamp(totalmillis - millis, 0, min(servpingdecay, totalmillis));
        if(millis >= lastreset && rtt < servpingdecay) si->addping(rtt, millis);
		si->numplayers = getint(p);
		int numattr = getint(p);
		si->attr.setsize(0);
        loopj(numattr) { int attr = getint(p); if(p.overread()) break; si->attr.add(attr); }
        getstring(text, p);
        filtertext(si->map, text, false);
        getstring(text, p);
        filtertext(si->sdesc, text);
    }
}

void sortservers()
{
    servers.sort(serverinfo::compare);
}
COMMAND(sortservers, "");

VARP(autosortservers, 0, 1, 1);
VARP(autoupdateservers, 0, 1, 1);

void refreshservers()
{
    static int lastrefresh = 0;
    if(lastrefresh==totalmillis) return;
    if(totalmillis - lastrefresh > 1000) 
    {
        loopv(servers) servers[i]->reset();
        lastreset = totalmillis;
    }
    lastrefresh = totalmillis;

    checkresolver();
    checkpings();
    if(totalmillis - lastinfo >= servpingrate/(maxservpings ? max(1, (servers.length() + maxservpings - 1) / maxservpings) : 1)) pingservers();
    if(autosortservers) sortservers();
}

serverinfo *selectedserver = NULL;

VARHSC(filtermmode, 0, 0, 3);
VARHSC(filterfull, 0, 0, 1);
VARHSC(filterempty, 0, 0, 1);
VARHSC(filterunknown, 0, 0, 1);
SVARF(filterdesc, "", loopi(strlen(filterdesc)) filterdesc[i] = tolower(filterdesc[i]));
SVARF(filtermap, "", loopi(strlen(filtermap)) filtermap[i] = tolower(filtermap[i]));

const char *showservers(g3d_gui *cgui, uint *header, int pagemin, int pagemax)
{
    refreshservers();
    if(servers.empty())
    {
        if(header) execute(header);
        return NULL;
    }
    serverinfo *sc = NULL;
	int no = 0;
    for(int start = 0; start < servers.length();)
    {
        if(start > 0) cgui->tab();
        if(header) execute(header);
        int end = servers.length();
        cgui->pushlist();
        loopi(geoipdisabled? 9: 10)
        {
            if(!game::serverinfostartcolumn(cgui, i)) break;
			no = 0;
            for(int j = start; j < end; j++)
            {
                if(!i && no >= pagemin && (no >= pagemax || cgui->shouldtab())) { end = j; break; }
                serverinfo &si = *servers[j];

				if (!si.attr.empty())
				{
					if (filtermmode && si.attr[4] >= filtermmode+1) continue;
					if (filterfull && si.attr[3] <= si.numplayers) continue;
					if (filterempty && si.numplayers == 0) continue;
				}
				if (filterunknown && (si.ping < 0 || si.attr.empty() || si.address.host == ENET_HOST_ANY || si.ping == INT_MAX || si.attr[0] != game::getprotocolversion())) continue;
				if (filterdesc[0])
				{
					int tlen = strlen(si.sdesc);
					char *tstr = new char[tlen+1];
					loopi(tlen) tstr[i] = tolower(si.sdesc[i]);
					tstr[tlen] = '\0';
					if (!strstr(tstr, filterdesc)) { delete tstr; continue; }
					delete tstr;
				}
				if (filtermap[0])
				{
					if (!strstr(si.map, filtermap)) continue;
				}
				no++;

                const char *sdesc = si.sdesc;
                if(si.address.host == ENET_HOST_ANY) sdesc = "[unknown host]";
                else if(si.ping == serverinfo::WAITING) sdesc = "[waiting for response]";
                if(game::serverinfoentry(cgui, i, si.name, si.port, sdesc, si.map, sdesc == si.sdesc ? si.ping : -1, si.attr, si.numplayers, si.country))
                    sc = &si;
            }
            game::serverinfoendcolumn(cgui, i);
        }
        cgui->poplist();
        start = end;
    }
	if (no < pagemax)
	{
		loopi(pagemax-no)
		{
			if (cgui->shouldtab()) break;
			cgui->text("", 0x000000, NULL);
		}
	}
    if(selectedserver || !sc) return NULL;
    selectedserver = sc;
    return "connectselected";
}

const char *quickserver(const char *name, const char *host, const int port)
{
	loopv(servers)
	{
		serverinfo *si = servers[i];
		if (!strcmp(si->name, host) && si->port == port && si->attr.length() >= 5)
		{
			char color;
			switch (si->attr[4])
			{
				case 2:			color = 'y'; break;
				case 3: case 4: color = 'r'; break;
				default:		color = 'w';
			}
			static string res;
			formatstring(res)("\f%c%s\fv(\fw%d/%d\fv) ", color, name, si->numplayers, si->attr[3]);
			return res;
		}
	}
	return name;
}
ICOMMAND(quickserver, "ssi", (const char *name, const char *host, const int *port), result(quickserver(name, host, *port)));

VARHSC(serverpreview, 0, 1, 1);
uint serverpreviewhost = 0;
int serverpreviewport = 0;
const char *extservinfo, *extservname, *extservpassword;

void connectselected(const int *force)
{
    if(!selectedserver) return;
	if (serverpreview && (!force || !(*force)))
	{
		if (selectedserver->address.host)
		{
			serverpreviewhost = selectedserver->address.host;
			serverpreviewport = selectedserver->port;
			extservpassword = selectedserver->password;
			extservname = selectedserver->name;
			game::clearextinfo();
			extservinfo = selectedserver->sdesc;
			execute("showgui extscoreboard");
		}
	}
    else
	{
		connectserv(selectedserver->name, selectedserver->port, selectedserver->password);
	}
	selectedserver = NULL;
}

COMMAND(connectselected, "i");
ICOMMAND(connectextselected, "", (), connectserv(extservname, serverpreviewport, extservpassword) );

void clearservers(bool full = false)
{
    resolverclear();
    if(full) servers.deletecontents();
    else loopvrev(servers) if(!servers[i]->keep) delete servers.remove(i);
    selectedserver = NULL;
	clearextplayers();
}

#define RETRIEVELIMIT 20000

void retrieveservers(vector<char> &data)
{
    ENetSocket sock = connectmaster();
    if(sock == ENET_SOCKET_NULL) return;

    extern char *mastername;
    defformatstring(text)("retrieving servers from %s... (esc to abort)", mastername);
    renderprogress(0, text);

    int starttime = SDL_GetTicks(), timeout = 0;
    const char *req = "list\n";
    int reqlen = strlen(req);
    ENetBuffer buf;
    while(reqlen > 0)
    {
        enet_uint32 events = ENET_SOCKET_WAIT_SEND;
        if(enet_socket_wait(sock, &events, 250) >= 0 && events) 
        {
            buf.data = (void *)req;
            buf.dataLength = reqlen;
            int sent = enet_socket_send(sock, NULL, &buf, 1);
            if(sent < 0) break;
            req += sent;
            reqlen -= sent;
            if(reqlen <= 0) break;
        }
        timeout = SDL_GetTicks() - starttime;
        renderprogress(min(float(timeout)/RETRIEVELIMIT, 1.0f), text);
        if(interceptkey(SDLK_ESCAPE)) timeout = RETRIEVELIMIT + 1;
        if(timeout > RETRIEVELIMIT) break;
    }

    if(reqlen <= 0) for(;;)
    {
        enet_uint32 events = ENET_SOCKET_WAIT_RECEIVE;
        if(enet_socket_wait(sock, &events, 250) >= 0 && events)
        {
            if(data.length() >= data.capacity()) data.reserve(4096);
            buf.data = data.getbuf() + data.length();
            buf.dataLength = data.capacity() - data.length();
            int recv = enet_socket_receive(sock, NULL, &buf, 1);
            if(recv <= 0) break;
            data.advance(recv);
        }
        timeout = SDL_GetTicks() - starttime;
        renderprogress(min(float(timeout)/RETRIEVELIMIT, 1.0f), text);
        if(interceptkey(SDLK_ESCAPE)) timeout = RETRIEVELIMIT + 1;
        if(timeout > RETRIEVELIMIT) break;
    }

    if(data.length()) data.add('\0');
    enet_socket_destroy(sock);
}

bool updatedservers = false;

void updatefrommaster()
{
    vector<char> data;
    retrieveservers(data);
    if(data.empty()) conoutf("master server not replying");
    else
    {
        clearservers();
        execute(data.getbuf());
    }
    refreshservers();
    updatedservers = true;
}

void initservers()
{
    selectedserver = NULL;
    if(autoupdateservers && !updatedservers) updatefrommaster();
}

struct playerextinfo { string name; int cn, ip, lastseen; serverinfo *serv; };
vector<playerextinfo> playereis;

void clearextplayers()
{
	playereis.setsize(0);
}

ENetSocket extinfosock = ENET_SOCKET_NULL;
int lastplayerinfo = -20000;
int lastinforesp = -20000;

VARHSC(playerrefreshinterval, 0, 15000, 60000);

bool updateextinfo(ENetSocket &socket, uint host, int port, ucharbuf &p)
{
	if(socket == ENET_SOCKET_NULL) 
	{
		socket = enet_socket_create(ENET_SOCKET_TYPE_DATAGRAM);
		if(socket == ENET_SOCKET_NULL)
		{
			return false;
		}
		enet_socket_set_option(socket, ENET_SOCKOPT_NONBLOCK, 1);
		enet_socket_set_option(socket, ENET_SOCKOPT_BROADCAST, 1);
	}
	ENetBuffer buf;

	if (host)
	{
		ENetAddress address;
		address.host = host;
		address.port = port;

		//if(address.host == ENET_HOST_ANY) return false;
		buf.data = p.buf;
		buf.dataLength = p.length();
		enet_socket_send(socket, &address, &buf, 1);
	}
	else
	{
		loopv(servers)
		{
			serverinfo &si = *servers[i];
			if(si.address.host == ENET_HOST_ANY) continue;
			buf.data = p.buf;
			buf.dataLength = p.length();
			enet_socket_send(socket, &si.address, &buf, 1);
		}
		if(searchlan)
		{
			ENetAddress address;
			address.host = ENET_HOST_BROADCAST;
			address.port = server::laninfoport();
			buf.data = p.buf;
			buf.dataLength = p.length();
			enet_socket_send(socket, &address, &buf, 1);
		}
	}
	return true;
}

void checkextinfo(ENetSocket &socket, bool (extinforesponse)(ucharbuf &p, ENetAddress &addr, int len))
{
	if(socket==ENET_SOCKET_NULL) return;
	enet_uint32 events = ENET_SOCKET_WAIT_RECEIVE;
	ENetBuffer buf;
	ENetAddress addr;
	uchar ping[MAXTRANS];
	buf.data = ping; 
	buf.dataLength = sizeof(ping);

	while(enet_socket_wait(socket, &events, 0) >= 0 && events)
	{
		int len = enet_socket_receive(socket, &addr, &buf, 1);
		if(len <= 0) return;

		ucharbuf p(ping, len);
		if (!extinforesponse(p, addr, len)) break;
	}
}

bool extplayerresponse(ucharbuf &p, ENetAddress &addr, int len)
{
	char text[MAXTRANS];
	getint(p); getint(p); getint(p);
	int ack = getint(p);
	int ver = getint(p);
	int iserr = getint(p);
	if (ack != EXT_ACK || ver != EXT_VERSION || iserr != EXT_NO_ERROR) return true;
	lastinforesp = lastmillis;

	int extresp = getint(p);
	if (extresp == EXT_PLAYERSTATS_RESP_STATS)
	{
		playerextinfo pei;
		pei.lastseen = totalmillis;
		loopv(servers) if(addr.host == servers[i]->address.host && addr.port == servers[i]->address.port) { pei.serv = servers[i]; break; }
		pei.cn = getint(p);
		getint(p);
		getstring(pei.name, p);
		getstring(text, p);
		getint(p);
		getint(p);
		getint(p);
		getint(p);
		getint(p);
		getint(p);
		getint(p);
		getint(p);
		getint(p);
		getint(p);
		p.get((uchar*)&pei.ip, 3);
		bool found = false;
		int servpi = playereis.length();
		loopv(playereis) if (playereis[i].serv == pei.serv && playereis[i].cn == pei.cn)
		{
			strcpy(playereis[i].name, pei.name);
			playereis[i].ip = pei.ip;
			playereis[i].lastseen = totalmillis;
			found = true;
		} else if (playereis[i].serv == pei.serv) servpi = i+1;
		if (!found)
		{
			playereis.insert(servpi, pei);
			playereis[servpi].serv = pei.serv;
			playereis[servpi].ip = pei.ip;
		}
		game::addwhoisentry(pei.ip, pei.name);
	}
	return true;
}

VARHSC(playerbrowsercountry, 0, 5, 5);
VARHSC(playerbrowsertype, 0, 0, 2);
SVAR(filtername, "");

const char *showplayers(g3d_gui *cgui, uint *header, int pagemin)
{
    refreshservers();

	if (lastmillis-lastplayerinfo > playerrefreshinterval)
	{
		uchar ping[MAXTRANS];
		ucharbuf p(ping, sizeof(ping));
		putint(p, 0);
		putint(p, EXT_PLAYERSTATS);
		putint(p, -1);
		updateextinfo(extinfosock, 0, 0, p);
		lastplayerinfo = lastmillis;
	}
	checkextinfo(extinfosock, extplayerresponse);

	loopv(playereis) if (totalmillis-playereis[i].lastseen > 30000) playereis.remove(i);

	vector<playerextinfo> *filteredpeis = &playereis;
	if (filtername[0]!=0 || playerbrowsertype>0)
	{
		filteredpeis = new vector<playerextinfo>;
		char tl[MAXSTRLEN], tr[MAXSTRLEN];
		strcpy(tr, filtername);
		strlwr(tr);
		loopv(playereis)
		{
			strcpy(tl, playereis[i].name); 
			strlwr(tl);
			if ((!filtername[0] || strstr(tl, tr)) /*&& (playerbrowsertype!=2 || 1)*/ && (playerbrowsertype!=1 || game::isfriendip(playereis[i].ip))) filteredpeis->add(playereis[i]);
		}
	}

	execute(header);

	serverinfo *selplserv = NULL;

	for (int i = 0; i < max(1, (filteredpeis->length()+pagemin-1)/(pagemin? pagemin: 1)); i++)
	{
		if (i)
		{
			cgui->tab();
			execute(header);
		}

		cgui->pushlist();
		//cgui->mergehits(1);

		cgui->pushlist();
		cgui->text("player", 0xFFFF80, 0);
		cgui->strut(25);
		for (int j = 0; j < min(filteredpeis->length()-i*pagemin, pagemin); j++) if(cgui->buttonf("%s ", 0xFFFFDD, NULL, (*filteredpeis)[j+i*pagemin].name)&G3D_UP)
		{
			game::whoisip((*filteredpeis)[j+i*pagemin].ip, (*filteredpeis)[j+i*pagemin].name);
		}
		cgui->poplist();

		cgui->pushlist();
		cgui->text("server", 0xFFFF80, 0);
		cgui->strut(30);
		for (int j = 0; j < min(filteredpeis->length()-i*pagemin, pagemin); j++)
		{
			serverinfo *si = (*filteredpeis)[j+i*pagemin].serv;
			string res;
			if (si->sdesc && si->sdesc[0]) formatstring(res)("\fw%s\fv ", si->sdesc);
			else formatstring(res)("%s %d ", si->name, si->port);

			if(cgui->buttonf("%s", 0xFFFFDD, NULL, res)&G3D_UP) selplserv = (*filteredpeis)[j+i*pagemin].serv;
		}
		cgui->poplist();

		if (!geoipdisabled && playerbrowsercountry > 0)
		{
			cgui->pushlist();
			cgui->text("country", 0xFFFF80, 0);
			cgui->strut(25);
			for (int j = 0; j < min(filteredpeis->length()-i*pagemin, pagemin); j++)
			{
				const char icon[MAXSTRLEN] = "";
				const char *countrycode = GeoIP_country_code_by_ipnum(geoip, endianswap((*filteredpeis)[j+i*pagemin].ip));
				const char *country = (playerbrowsercountry&2)? countrycode:
										(playerbrowsercountry&4)? GeoIP_country_name_by_ipnum(geoip, endianswap((*filteredpeis)[j+i*pagemin].ip)): "";
				if (playerbrowsercountry&1) formatstring(icon)("%s.png", countrycode);
				if (!countrycode) countrycode = country = "unknown";
				cgui->textf("%s", 0xFFFFDD, (playerbrowsercountry&1)? icon: NULL, country);
			}
		}

		if (!i && filteredpeis->length() < pagemin) for (int j = filteredpeis->length(); j < pagemin; j++) cgui->text("", 0xFFFF80, 0);
		if (!geoipdisabled && playerbrowsercountry > 0) cgui->poplist();
		
		//cgui->mergehits(0);
		cgui->poplist();
	}
	if (filteredpeis != &playereis) delete filteredpeis;
	if (selectedserver || !selplserv) return NULL;
	selectedserver = selplserv;
	return "connectselected";
}

ICOMMAND(refreshextplayers, "", (), { playereis.setsize(0); lastplayerinfo = -20000; });
ICOMMAND(extplayerstotal, "", (), { intret(playereis.length()); });

ICOMMAND(addserver, "sis", (const char *name, int *port, const char *password), addserver(name, *port, password[0] ? password : NULL));
ICOMMAND(keepserver, "sis", (const char *name, int *port, const char *password), addserver(name, *port, password[0] ? password : NULL, true));
ICOMMAND(clearservers, "i", (int *full), clearservers(*full!=0));
COMMAND(updatefrommaster, "");
COMMAND(initservers, "");

void writeservercfg()
{
    if(!game::savedservers()) return;
    stream *f = openutf8file(path(game::savedservers(), true), "w");
    if(!f) return;
    int kept = 0;
    loopv(servers)
    {
        serverinfo *s = servers[i];
        if(s->keep)
        {
            if(!kept) f->printf("// servers that should never be cleared from the server list\n\n");
            if(s->password) f->printf("keepserver %s %d %s\n", escapeid(s->name), s->port, escapestring(s->password));
            else f->printf("keepserver %s %d\n", escapeid(s->name), s->port);
            kept++;
        }
    }
    if(kept) f->printf("\n");
    f->printf("// servers connected to are added here automatically\n\n");
    loopv(servers) 
    {
        serverinfo *s = servers[i];
        if(!s->keep) 
        {
            if(s->password) f->printf("addserver %s %d %s\n", escapeid(s->name), s->port, escapestring(s->password));
            else f->printf("addserver %s %d\n", escapeid(s->name), s->port);
        }
    }
    delete f;
}

SVAR(demolist, "");
VAR(selecteddemo, 0, 0, 1);
VARHSC(maxnodemos, 1, 340, 1000);
string demomode;
struct demoinfo { char *file, *map, *date; int size; };
vector<demoinfo> demofilelist;

struct demoheader
{
    char magic[16];
    int version, protocol;
};


void getdemomap(const char *file, char *mapname)
{
	mapname[0] = '\0';

    stream *f = opengzfile(file, "rb");
    if(!f) return;

    demoheader hdr;
	f->read(&hdr, sizeof(demoheader));
	int nextplayback, chan, len;
	f->read(&nextplayback, sizeof(nextplayback));
	f->read(&chan, sizeof(chan));
	f->read(&len, sizeof(len));

	ENetPacket *packet = enet_packet_create(NULL, len, 0);
	f->read(packet->data, len);
	packetbuf p(packet);
	int x = getint(p);
	int o = getint(p);
	int u = getint(p);
	x = getint(p);
	o = getint(p);
	//u = getint(p);
	getstring(mapname, p, MAXSTRLEN);
    sendpacket(-1, chan, packet);
	enet_packet_destroy(packet);

	//conoutf("%d", nextplayback);
	//string tmp;
	//f->read(tmp, 16);
	//conoutf(tmp);
	//f->seek(44, 0);
	//int len;
	//f->read(&len, sizeof(len));
	//lilswap(&len, 1);
	//f->read(mapname, len);
	f->close();

	DELETEP(f);
}

void initdemos(const char *mode)
{
	strcpy(demomode, mode);
	loopv(demofilelist)
	{
		delete[] demofilelist[i].file;
		delete[] demofilelist[i].map;
		delete[] demofilelist[i].date;
	}
	demofilelist.setsize(0);
	defformatstring(dir)("demos/%s", mode);
	vector<char *> files;
    listfiles(dir, "dmo", files);
	for (int i = files.length()-1; i > max(files.length()-maxnodemos, 0); i--)
	{
		demoinfo di;
		di.file = files[i];
		di.date = new char[MAXSTRLEN];
		formatstring(di.date)("%.19s", files[i]);
		di.map = new char[MAXSTRLEN];
		defformatstring(dipath)("%s/%s.dmo", dir, files[i]);
		getdemomap(dipath, di.map);
		demofilelist.add(di);
	}
}

COMMAND(initdemos, "s");

const char *showdemos(g3d_gui *cgui, uint *header, int pagemin)
{
	execute(header);
	static string cmd;
	int seldemo = -1;

	for (int i = 0; i < max(1, (demofilelist.length()+pagemin-1)/(pagemin? pagemin: 1)); i++)
	{
		if (i)
		{
			cgui->tab();
			execute(header);
		}

		cgui->pushlist();
		cgui->mergehits(1);

		cgui->pushlist();
		cgui->text("server", 0xFFFF80, 0);
		cgui->strut(30);
		for (int j = 0; j < min(demofilelist.length()-i*pagemin, pagemin); j++) if (cgui->button(demofilelist[j+i*pagemin].file, 0xFFFFDD)&G3D_UP) seldemo = j+i*pagemin;
		cgui->poplist();

		cgui->pushlist();
		cgui->text("map", 0xFFFF80, 0);
		cgui->strut(15);
		for (int j = 0; j < min(demofilelist.length()-i*pagemin, pagemin); j++) cgui->textf("%.19s", 0xFFFFDD, 0, demofilelist[j+i*pagemin].map);
		if (!i && demofilelist.length() < pagemin) for (int j = demofilelist.length(); j < pagemin; j++) cgui->text("", 0xFFFF80, 0);
		//for (int j = 0; j < min(filteredpeis->length()-i*pagemin, pagemin); j++) cgui->buttonf("%s ", 0xFFFFDD, NULL, (*filteredpeis)[j+i*pagemin].serv->sdesc? (*filteredpeis)[j+i*pagemin].serv->sdesc: "");
		cgui->poplist();

		cgui->pushlist();
		cgui->text("time", 0xFFFF80, 0);
		cgui->strut(25);
		for (int j = 0; j < min(demofilelist.length()-i*pagemin, pagemin); j++) cgui->textf("%.19s", 0xFFFFDD, 0, demofilelist[j+i*pagemin].date);
		if (!i && demofilelist.length() < pagemin) for (int j = demofilelist.length(); j < pagemin; j++) cgui->text("", 0xFFFF80, 0);
		cgui->poplist();

		//cgui->pushlist();
		//cgui->text("size", 0xFFFF80, 0);
		//cgui->strut(25);
		//for (int j = 0; j < min(demofilelist.length()-i*pagemin, pagemin); j++) cgui->textf("%.19s", 0xFFFFDD, 0, demofilelist[j+i*pagemin]);
		//if (!i && demofilelist.length() < pagemin) for (int j = demofilelist.length(); j < pagemin; j++) cgui->text("", 0xFFFF80, 0);
		//cgui->poplist();
		
		cgui->mergehits(0);
		cgui->poplist();
	}
	if (selecteddemo || seldemo == -1) return NULL;
	selecteddemo = seldemo;
	formatstring(cmd)("setmode -1; demo demos/%s/%s; selecteddemo 0", demomode, demofilelist[seldemo].file);
	return cmd;
}
