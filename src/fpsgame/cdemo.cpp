#include "game.h"
#include "cdemo.h"

namespace game{
extern int mastermode, gamespeed;
}

namespace cdemo{

static stream* demostream = NULL;
static string name;
static int starttime = 0;

static uchar somespace[MAXTRANS];
static ucharbuf p(somespace, MAXTRANS);

const char *keep_client_demo_gui = 
"newgui keepdemogui [\n"
	"guititle \"Keep demo?\"\n"
	"guialign 0 [\n"
        "guilist [\n"
            "guibar\n"
            "guitext \"^f7Automatic client demo recording is ^f0active.\"\n"
            "guitext \"^f7Would you like to ^f0keep ^f7the last recorded demo?\"\n"
            "guistrut\n"
            "guibar\n"
            "guilist [\n"
                "guistrut\n"
                "guitext \"         \"\n"
                "guibutton \"^f0Yes ^f7(default) \" [keepdemo 1]\n"
                "guistrut\n"
                "guibutton \"^f3No \" [keepdemo 0]\n"
                "guistrut\n"
            "]\n"
            "guibar\n"
        "]\n"
	"]\n"
"] \"Client Demo\"\n";

static void writedemo(int chan, void* data, int len)
{
       if(!demostream) return;
       int stamp[3] = { lastmillis - starttime, chan, len };
       lilswap(stamp, 3);
       demostream->write(stamp, sizeof(stamp));
       demostream->write(data, len);
}

SVARHSC(cdemodir, "demos");

void setup(const char* name_)
{
       using game::gamemode;
       using game::players;
       using entities::ents;

       if(!m_mp(gamemode) || game::demoplayback || !game::connected) return;
       if(demostream) stop();

	   if (cdemodir[0])
	   {
			const char *sdir = findfile(cdemodir, "w");
			defformatstring(dbuf)("%s/", sdir);
			if(!fileexists(dbuf, "w"))
			{
				createdir(dbuf);
			}
	   }

       string defname={'\0'};
       game::concatgamedesc(defname);
       formatstring(name)("%s%s%s.dmo", cdemodir[0]? cdemodir: "", cdemodir[0]? "/": "", name_ && name_[0] ? name_ : defname);
       demostream = opengzfile(name, "w+b");
       if(!demostream) return;

       starttime = lastmillis;
       demoheader hdr;
       memcpy(hdr.magic, DEMO_MAGIC, sizeof(hdr.magic));
       hdr.version = DEMO_VERSION;
       hdr.protocol = PROTOCOL_VERSION;
       lilswap(&hdr.version, 2);
       demostream->write(&hdr, sizeof(demoheader));

       p.len = 0;
       //welcomepacket(), see fpsgame/server.cpp
       putint(p, N_WELCOME);
       putint(p, N_MAPCHANGE);
       sendstring(game::getclientmap(), p);
       putint(p, gamemode);
       putint(p, 0);
       putint(p, N_ITEMLIST);
       loopv(ents)
               if(ents[i]->type>=I_SHELLS && ents[i]->type<=I_QUAD && (!m_noammo || ents[i]->type<I_SHELLS || ents[i]->type>I_CARTRIDGES)
               && ents[i]->spawned){
                       putint(p, i);
                       putint(p, ents[i]->type);
       }
       putint(p, -1);
       putint(p, N_TIMEUP);
       putint(p, lastmillis < game::maplimit && !game::intermission ? max((game::maplimit - lastmillis)/1000, 1) : 0);
    bool hasmaster = false;
    if(game::mastermode != MM_OPEN)
    {
        putint(p, N_CURRENTMASTER);
        putint(p, game::mastermode);
        hasmaster = true;
    }
    loopv(players) if(players[i]->privilege >= PRIV_MASTER)
    {
        if(!hasmaster)
        {
            putint(p, N_CURRENTMASTER);
            putint(p, game::mastermode);
            hasmaster = true;
        }
        putint(p, players[i]->clientnum);
        putint(p, players[i]->privilege);
    }
    if(hasmaster) putint(p, -1);
       if(game::ispaused())
       {
               putint(p, N_PAUSEGAME);
               putint(p, 1);
       }
    if(game::gamespeed != 100)
    {
        putint(p, N_GAMESPEED);
        putint(p, game::gamespeed);
        putint(p, -1);
    }
    if(m_teammode)
    {
        putint(p, N_TEAMINFO);
        enumerates(game::teaminfos, teaminfo, t,
            if(t.frags) { sendstring(t.team, p); putint(p, t.frags); }
        );
        sendstring("", p);
    }
       putint(p, N_RESUME);
       loopv(players)
       {
               fpsent* d = players[i];
               putint(p, d->clientnum);
               putint(p, d->state);
               putint(p, d->frags);
               putint(p, d->flags);
               putint(p, d->quadmillis);
               putint(p, d->lifesequence);
               putint(p, d->health);
               putint(p, d->maxhealth);
               putint(p, d->armour);
               putint(p, d->armourtype);
               putint(p, d->gunselect);
               loopi(GUN_PISTOL-GUN_SG+1) putint(p, d->ammo[GUN_SG+i]);
       }
       putint(p, -1);
       loopv(players)
       {
               fpsent* d = players[i];
               if(d->aitype != AI_NONE)
               {
                       putint(p, N_INITAI);
                       putint(p, d->clientnum);
                       putint(p, d->ownernum);
                       putint(p, d->aitype);
                       putint(p, d->skill);
                       putint(p, d->playermodel);
                       sendstring(d->name, p);
                       sendstring(d->team, p);
               }
               else
               {
                       putint(p, N_INITCLIENT);
                       putint(p, d->clientnum);
                       sendstring(d->name, p);
                       sendstring(d->team, p);
                       putint(p, d->playermodel);
               }
       }
       if(m_ctf) ctfinit(p);
       if(m_capture) captureinit(p);
       if(m_collect) collectinit(p);

	   putint(p, N_SERVCMD);
	   putint(p, HSC_PROTOCOL_VERSION);
	   putint(p, 1);
	   sendstring(game::servinfo, p);

       putint(p, N_SERVMSG);
       string info;
       if(name_ && name_[0]) formatstring(info)("original filename: %s.dmo (canonical: %s.dmo)", name, defname);
       else formatstring(info)("original filename: %s", name);
       sendstring(info, p);
       packet(1, p);

	   ::executestr(keep_client_demo_gui);
       conoutf("\f3recording client demo");

}

ENetPacket* packet(int chan, const ENetPacket* p)
{
       if(demostream) writedemo(chan, p->data, p->dataLength);
       return const_cast<ENetPacket*>(p);
}

void packet(int chan, const ucharbuf& p)
{
       if(!demostream) return;
       writedemo(chan, p.buf, p.len);
}

void extpacket(const ucharbuf& o)
{
       if(!demostream) return;
       p.len = 0;
	   putint(p, N_SERVCMD);
	   putint(p, HSC_PROTOCOL_VERSION);
	   putint(p, 0);
	   p.put(o.buf, o.len);
       packet(1, p);
}

static int skiptextonce = 0;
void addmsghook(int type, int cn, const ucharbuf& addmsgp)
{
       if(!demostream) return;
       switch(type){
       case N_TEXT:
               if(skiptextonce){
                       skiptextonce = 0;
                       return;
               }
       case N_GUNSELECT:
       case N_SWITCHTEAM:
       case N_SWITCHMODEL:
       case N_CLIENTPING:
       case N_SOUND:
       case N_TAUNT:
       case N_EDITMODE:
       case N_EDITENT:
       case N_EDITF:
       case N_EDITT:
       case N_EDITM:
       case N_FLIP:
       case N_COPY:
       case N_PASTE:
       case N_ROTATE:
       case N_REPLACE:
       case N_DELCUBE:
       case N_REMIP:
       case N_EDITVAR:
               p.len=0;
               putint(p, N_CLIENT);
               putint(p, cn==-1?int(game::player1->clientnum):cn);
               putint(p, addmsgp.len);
               p.put(addmsgp.buf, addmsgp.len);
               packet(1, p);
       default: return;
       }
}

void sayteam(const char* msg)
{
       if(!demostream) return;
       p.len = 0;
       putint(p, game::player1->clientnum);
       sendstring(msg, p);
       packet(1, p);
}

void clipboard(int plainlen, int packlen, const uchar* data)
{
       if(!demostream) return;
       packetbuf q(32 + packlen);
       putint(q, N_CLIPBOARD);
       putint(q, game::player1->clientnum);
       putint(q, plainlen);
       putint(q, packlen);
       q.put(data, packlen);
       packet(1, q);
}

void explodefx(int cn, int gun, int id)
{
       if(!demostream) return;
       p.len = 0;
       putint(p, N_EXPLODEFX);
       putint(p, cn);
       putint(p, gun);
       putint(p, id);
       packet(1, p);
}

void shotfx(int cn, int gun, int id, const vec& from, const vec& to)
{
    if(!demostream) return;
    p.len = 0;
    putint(p, N_SHOTFX);
    putint(p, cn);
    putint(p, gun);
    putint(p, id);
    loopi(3) putint(p, int(from[i]*DMF));
    loopi(3) putint(p, int(to[i]*DMF));
    packet(1, p);
}

string curname;
void stop()
{
    if(!demostream) return;
    DELETEP(demostream);
    copystring(curname, name);
    if(cdemoauto == 1) showgui("keepdemogui");
    else conoutf("\f0recorded client demo (%s)", name);
}

VARHSC(cdemoauto, 0, 0, 2);
COMMANDN(cdemostart, setup, "s");
COMMANDN(cdemostop, stop, "");
ICOMMAND(say_nocdemo, "s", (char* s), skiptextonce = 1; game::toserver(s); );

void keepdemo(int *keep)
{
    string nil;
    copystring(nil, "null");
    if (*keep == 0)
    { // remove demo
        if (strcmp(curname, nil) != 0)
        {
            int deleted;
            string tmp;
            copystring(tmp, homedir);
            concatstring(tmp, curname);
            deleted = remove(tmp);
            if (deleted != 0) {
                conoutf("\f3WARNING: Could not delete %s", curname);
            }
            else
            {
                conoutf("\f0Successfully deleted auto-recorded demo: %s", curname);
                if (!game::connected) copystring(curname, nil);
            }
        }
        else
        {
            conoutf("\f3WARNING: Empty demo name. No demo recorded or already erased!");
            if (!game::connected) copystring(curname, nil);
        }
    } 
    else
    {
        // keep demo
        conoutf("\f0kept recorded client demo (%s)", curname);
        if (!game::connected) copystring(curname, nil);
    }
}

COMMAND(keepdemo,"i");

}
