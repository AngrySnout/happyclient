// creation of scoreboard
#include "game.h"

namespace game
{
    VARP(scoreboard2d, 0, 1, 1);
    VARP(showservinfo, 0, 1, 1);
    VARP(showclientnum, 0, 0, 1);
    VARP(showpj, 0, 0, 1);
    VARP(showping, 0, 1, 1);
    VARP(showspectators, 0, 1, 1);
    VARP(highlightscore, 0, 1, 1);
    VARP(showconnecting, 0, 0, 1);

    static hashset<teaminfo> teaminfos;

    void clearteaminfo()
    {
        teaminfos.clear();
    }

    void setteaminfo(const char *team, int frags)
    {
        teaminfo *t = teaminfos.access(team);
        if(!t) { t = &teaminfos[team]; copystring(t->team, team, sizeof(t->team)); }
        t->frags = frags;
    }
            
    static inline bool playersort(const fpsent *a, const fpsent *b)
    {
        if(a->state==CS_SPECTATOR)
        {
            if(b->state==CS_SPECTATOR) return strcmp(a->name, b->name) < 0;
            else return false;
        }
        else if(b->state==CS_SPECTATOR) return true;
        if(m_ctf || m_collect)
        {
            if(a->flags > b->flags) return true;
            if(a->flags < b->flags) return false;
        }
        if(a->frags > b->frags) return true;
        if(a->frags < b->frags) return false;
        return strcmp(a->name, b->name) < 0;
    }

    void getbestplayers(vector<fpsent *> &best)
    {
        loopv(players)
        {
            fpsent *o = players[i];
            if(o->state!=CS_SPECTATOR) best.add(o);
        }
        best.sort(playersort);
        while(best.length() > 1 && best.last()->frags < best[0]->frags) best.drop();
    }

    void getbestteams(vector<const char *> &best)
    {
        if(cmode && cmode->hidefrags()) 
        {
            vector<teamscore> teamscores;
            cmode->getteamscores(teamscores);
            teamscores.sort(teamscore::compare);
            while(teamscores.length() > 1 && teamscores.last().score < teamscores[0].score) teamscores.drop();
            loopv(teamscores) best.add(teamscores[i].team);
        }
        else 
        {
            int bestfrags = INT_MIN;
            enumerates(teaminfos, teaminfo, t, bestfrags = max(bestfrags, t.frags));
            if(bestfrags <= 0) loopv(players)
            {
                fpsent *o = players[i];
                if(o->state!=CS_SPECTATOR && !teaminfos.access(o->team) && best.htfind(o->team) < 0) { bestfrags = 0; best.add(o->team); } 
            }
            enumerates(teaminfos, teaminfo, t, if(t.frags >= bestfrags) best.add(t.team));
        }
    }

    struct scoregroup : teamscore
    {
        vector<fpsent *> players;
    };
    static vector<scoregroup *> groups;
    static vector<fpsent *> spectators;

    static inline bool scoregroupcmp(const scoregroup *x, const scoregroup *y)
    {
        if(!x->team)
        {
            if(y->team) return false;
        }
        else if(!y->team) return true;
        if(x->score > y->score) return true;
        if(x->score < y->score) return false;
        if(x->players.length() > y->players.length()) return true;
        if(x->players.length() < y->players.length()) return false;
        return x->team && y->team && strcmp(x->team, y->team) < 0;
    }

    static int groupplayers()
    {
        int numgroups = 0;
        spectators.setsize(0);
        loopv(players)
        {
            fpsent *o = players[i];
            if(!showconnecting && !o->name[0]) continue;
            if(o->state==CS_SPECTATOR) { spectators.add(o); continue; }
            const char *team = m_teammode && o->team[0] ? o->team : NULL;
            bool found = false;
            loopj(numgroups)
            {
                scoregroup &g = *groups[j];
                if(team!=g.team && (!team || !g.team || strcmp(team, g.team))) continue;
                g.players.add(o);
                found = true;
            }
            if(found) continue;
            if(numgroups>=groups.length()) groups.add(new scoregroup);
            scoregroup &g = *groups[numgroups++];
            g.team = team;
            if(!team) g.score = 0;
            else if(cmode && cmode->hidefrags()) g.score = cmode->getteamscore(o->team);
            else { teaminfo *ti = teaminfos.access(team); g.score = ti ? ti->frags : 0; }
            g.players.setsize(0);
            g.players.add(o);
        }
        loopi(numgroups) groups[i]->players.sort(playersort);
        spectators.sort(playersort);
        groups.sort(scoregroupcmp, 0, numgroups);
        return numgroups;
    }

	VARHSC(scoreboardextinfo, 0, 1, 1);
	VARHSC(scoreboardfrags, 0, 1, 1);
	VARHSC(scoreboardflag, 0, 1, 1);
	VARHSC(scoreboardflags, 0, 1, 1);
	VARHSC(scoreboarddeaths, 0, 1, 1);
	VARHSC(scoreboardaccuracy, 0, 1, 1);
	VARHSC(scoreboardkpd, 0, 0, 1);
	VARHSC(scoreboardteamkills, 0, 0, 1);
	VARHSC(scoreboardcountry, 0, 3, 5);
	VARHSC(scoreboardspecping, 0, 1, 1);

    void renderscoreboard(g3d_gui &g, bool firstpass)
    {
		bool showextinfo = (lastmillis-lastinforesp < 20000) && scoreboardextinfo;
        const ENetAddress *address = connectedpeer();
        if(showservinfo && address)
        {
            string hostname;
            if(enet_address_get_host_ip(address, hostname, sizeof(hostname)) >= 0)
            {
                if(servinfo[0]) g.titlef("%.25s", 0xFFFF80, NULL, servinfo);
                else g.titlef("%s:%d", 0xFFFF80, NULL, hostname, address->port);
            }
        }
     
        g.pushlist();
        g.spring();
        g.text(server::modename(gamemode), 0xFFFF80);
        g.separator();
        const char *mname = getclientmap();
        g.text(mname[0] ? mname : "[new map]", 0xFFFF80);
        extern int gamespeed;
        if(gamespeed != 100) { g.separator(); g.textf("%d.%02dx", 0xFFFF80, NULL, gamespeed/100, gamespeed%100); }
        if(m_timed && mname[0] && (maplimit >= 0 || intermission))
        {
            g.separator();
            if(intermission) g.text("intermission", 0xFFFF80);
            else 
            {
                int secs = max(maplimit-lastmillis, 0)/1000, mins = secs/60;
                secs %= 60;
                g.pushlist();
                g.strut(mins >= 10 ? 4.5f : 3.5f);
                g.textf("%d:%02d", 0xFFFF80, NULL, mins, secs);
                g.poplist();
            }
        }
        if(ispaused()) { g.separator(); g.text("paused", 0xFFFF80); }
        g.spring();
        g.poplist();

        g.separator();
 
        int numgroups = groupplayers();
        loopk(numgroups)
        {
            if((k%2)==0) g.pushlist(); // horizontal
            
            scoregroup &sg = *groups[k];
            int bgcolor = sg.team && m_teammode ? (isteam(player1->team, sg.team) ? 0x3030C0 : 0xC03030) : 0,
                fgcolor = 0xFFFF80;

            g.pushlist(); // vertical
            g.pushlist(); // horizontal

            #define loopscoregroup(o, b) \
                loopv(sg.players) \
                { \
                    fpsent *o = sg.players[i]; \
                    b; \
                }    

            g.pushlist();
            if(sg.team && m_teammode)
            {
                g.pushlist();
                g.background(bgcolor, numgroups>1 ? 3 : 5);
                g.strut(1);
                g.poplist();
            }
            g.text("", 0, " ");
            loopscoregroup(o,
            {
                if(o==player1 && highlightscore && (multiplayer(false) || demoplayback || players.length() > 1))
                {
                    g.pushlist();
                    g.background(0x808080, numgroups>1 ? 3 : 5);
                }
                const playermodelinfo &mdl = getplayermodelinfo(o);
                const char *icon = sg.team && m_teammode ? (isteam(player1->team, sg.team) ? mdl.blueicon : mdl.redicon) : mdl.ffaicon;
				if (scoreboardflag && (m_ctf||m_protect) && (cmode->getflagowner(0) == o || cmode->getflagowner(1) == o)) icon = (isteam(o->team, player1->team)^m_protect? "blip_red_flag.png": "blip_blue_flag.png");
				g.text("", 0, icon);
                if(o==player1 && highlightscore && (multiplayer(false) || demoplayback || players.length() > 1)) g.poplist();
            });
            g.poplist();

            if(sg.team && m_teammode)
            {
                g.pushlist(); // vertical

                if(sg.score>=10000) g.textf("%s: WIN", fgcolor, NULL, sg.team);
                else g.textf("%s: %d", fgcolor, NULL, sg.team, sg.score);

                g.pushlist(); // horizontal
            }

            if(scoreboardfrags || !cmode || !cmode->hidefrags())
            {
				g.pushlist();
				g.strut(6);
				g.text("frags", fgcolor);
				loopscoregroup(o,
				{
					if (scoreboardflags && o->flags) g.textf("%d-%d", 0xFFFFDD, NULL, o->frags, o->flags);
					else g.textf("%d", 0xFFFFDD, NULL, o->frags);
				});
				g.poplist();
            }

			if (showextinfo)
			{
				if(scoreboarddeaths)
				{
					g.pushlist();
					g.strut(6);
					g.text("deaths", fgcolor);
					loopscoregroup(o, g.textf("%d", 0xFFFFDD, NULL, o->deaths));
					g.poplist();
				}

				if(scoreboardaccuracy)
				{
					g.pushlist();
					g.strut(6);
					g.text("acc", fgcolor);
					loopscoregroup(o, g.textf("%d%%", 0xFFFFDD, NULL, o->ext_accuracy));
					g.poplist();
				}

				if(scoreboardkpd)
				{
					g.pushlist();
					g.strut(6);
					g.text("kpd", fgcolor);
					loopscoregroup(o, g.textf("%.2f", 0xFFFFDD, NULL, (float)o->frags/max(o->deaths, 1)));
					g.poplist();
				}

				if(scoreboardteamkills)
				{
					g.pushlist();
					g.strut(6);
					g.text("tks", fgcolor);
					loopscoregroup(o, g.textf("%d", 0xFFFFDD, NULL, o->ext_teamkills));
					g.poplist();
				}
			}

            g.pushlist();
            g.text("name", fgcolor);
            g.strut(13);
            loopscoregroup(o, 
            {
                int status = o->state!=CS_DEAD ? 0xFFFFDD : 0x606060;
                if(o->privilege)
                {
                    status = (o->privilege>=PRIV_ADMIN) ? 0xFF8000 : (o->privilege==PRIV_AUTH)? 0x9040FF: 0x40FF80;
                    if(o->state==CS_DEAD) status = (status>>1)&0x7F7F7F;
                }
                g.textf("%s ", status, NULL, colorname(o));
            });
            g.poplist();

            if(multiplayer(false) || demoplayback)
            {
                if(showpj)
                {
                    g.pushlist();
                    g.strut(6);
                    g.text("pj", fgcolor);
                    loopscoregroup(o,
                    {
                        if(o->state==CS_LAGGED) g.text("LAG", 0xFFFFDD);
                        else g.textf("%d", 0xFFFFDD, NULL, o->plag);
                    });
                    g.poplist();
                }

                if(showping)
                {
                    g.pushlist();
                    g.text("ping", fgcolor);
                    g.strut(6);
                    loopscoregroup(o,
                    {
                        fpsent *p = o->ownernum >= 0 ? getclient(o->ownernum) : o;
                        if(!p) p = o;
                        if(!showpj && p->state==CS_LAGGED) g.text("LAG", 0xFFFFDD);
                        else g.textf("%d", 0xFFFFDD, NULL, p->ping);
                    });
                    g.poplist();
                }
            }

            if(showclientnum || player1->privilege>=PRIV_MASTER)
            {
                g.pushlist();
				g.strut(3);
                g.text("cn", fgcolor);
                loopscoregroup(o, g.textf("%d", 0xFFFFDD, NULL, o->clientnum));
                g.poplist();
            }

			if(showextinfo && scoreboardcountry)
            {
                g.space(1);
				g.pushlist();
				g.strut(6);
				g.text("country", fgcolor);
				loopscoregroup(o,
								{
									const char icon[MAXSTRLEN] = "";
									const char *countrycode = GeoIP_country_code_by_ipnum(geoip, endianswap(o->ip));
									const char *country = (scoreboardcountry&2)? countrycode:
														  (scoreboardcountry&4)? GeoIP_country_name_by_ipnum(geoip, endianswap(o->ip)): "";
									if (scoreboardcountry&1) formatstring(icon)("%s.png", countrycode);
									g.textf("%s", 0xFFFFDD, (scoreboardcountry&1)? icon: NULL, country);
								}
							  );
				g.poplist();
            }
            
            if(sg.team && m_teammode)
            {
                g.poplist(); // horizontal
                g.poplist(); // vertical
            }

            g.poplist(); // horizontal
            g.poplist(); // vertical

            if(k+1<numgroups && (k+1)%2) g.space(2);
            else g.poplist(); // horizontal
        }
        
        if(showspectators && spectators.length())
        {
			g.pushlist();
                
			g.pushlist();
			g.text("spectator", 0xFFFF80, " ");
			loopv(spectators) 
			{
				fpsent *o = spectators[i];
				int status = 0xFFFFDD;
				if(o->privilege) status = o->privilege>=PRIV_ADMIN ? 0xFF8000 : 0x40FF80;
				if(o==player1 && highlightscore)
				{
					g.pushlist();
					g.background(0x808080, 3);
				}
				g.text(colorname(o), status, "spectator");
				if(o==player1 && highlightscore) g.poplist();
			}
			g.poplist();

			if (scoreboardspecping)
			{
				g.space(1);
				g.pushlist();
				g.strut(3);
				g.text("ping", 0xFFFF80);
				loopv(spectators) g.textf("%d", 0xFFFFDD, NULL, spectators[i]->ping);
				g.poplist();
			}

            if(showclientnum || player1->privilege>=PRIV_MASTER)
            {
                g.space(1);
                g.pushlist();
                g.strut(3);
				g.text("cn", 0xFFFF80);
                loopv(spectators) g.textf("%d", 0xFFFFDD, NULL, spectators[i]->clientnum);
                g.poplist();
            }

			if(showextinfo && scoreboardcountry)
			{
				g.pushlist();
				g.text("country", 0xFFFF80);
				loopv(spectators)
								{
									const char icon[MAXSTRLEN] = "";
									const char *countrycode = GeoIP_country_code_by_ipnum(geoip, endianswap(spectators[i]->ip));
									const char *country = (scoreboardcountry&2)? countrycode:
															(scoreboardcountry&4)? GeoIP_country_name_by_ipnum(geoip, endianswap(spectators[i]->ip)): "";
									if (scoreboardcountry&1) formatstring(icon)("%s.png", countrycode);
									g.textf("%s ", 0xFFFFDD, (scoreboardcountry&1)? icon: NULL, country);
								}
				//g.space(1);
				g.poplist();
			}

            g.poplist();
            //else
            //{
            //    g.textf("%d spectator%s", 0xFFFF80, " ", spectators.length(), spectators.length()!=1 ? "s" : "");
            //    loopv(spectators)
            //    {
            //        if((i%3)==0) 
            //        {
            //            g.pushlist();
            //            g.text("", 0xFFFFDD, "spectator");
            //        }
            //        fpsent *o = spectators[i];
            //        int status = 0xFFFFDD;
            //        if(o->privilege) status = (o->privilege>=PRIV_ADMIN) ? 0xFF8000 : (o->privilege==PRIV_AUTH)? 0x9040FF: 0x40FF80;
            //        if(o==player1 && highlightscore)
            //        {
            //            g.pushlist();
            //            g.background(0x808080);
            //        }
            //        g.text(colorname(o), status);
            //        if(o==player1 && highlightscore) g.poplist();
            //        if(i+1<spectators.length() && (i+1)%3) g.space(1);
            //        else g.poplist();
            //    }
            //}
        }
    }

    struct scoreboardgui : g3d_callback
    {
        bool showing;
        vec menupos;
        int menustart;

        scoreboardgui() : showing(false) {}

        void show(bool on)
        {
            if(!showing && on)
            {
                menupos = menuinfrontofplayer();
                menustart = starttime();
            }
            showing = on;
        }

        void gui(g3d_gui &g, bool firstpass)
        {
            g.start(menustart, 0.03f, NULL, false);
            renderscoreboard(g, firstpass);
            g.end();
        }

        void render()
        {
            if(showing) g3d_addgui(this, menupos, (scoreboard2d ? GUI_FORCE_2D : GUI_2D | GUI_FOLLOW) | GUI_BOTTOM);
        }

    } scoreboard;

    void g3d_gamemenus()
    {
        scoreboard.render();
    }

    VARFN(scoreboard, showscoreboard, 0, 0, 1, scoreboard.show(showscoreboard!=0));

    void showscores(bool on)
    {
        showscoreboard = on ? 1 : 0;
        scoreboard.show(on);
    }
    ICOMMAND(showscores, "D", (int *down), showscores(*down!=0));

	ENetSocket extinfosock_ = ENET_SOCKET_NULL;
	int lastinfo_ = -20000;
	int lastinforesp_ = -20000;

	string extdesc, extmap;
	int extnumplayers, extmaxplayers, extpaused, extgamemode, extgamelimit, extgamespeed, extmastermode;

	struct extplayer_ { string name, team; int cn, ip, lastseen, frags, flags, deaths, acc, ping, state, privilege, tks; float kpd; };
	vector<extplayer_> extplayers;

	struct extscoregroup : teamscore
    {
        vector<extplayer_ *> players;
    };
    static vector<extscoregroup *> extgroups;
    static vector<extplayer_ *> extspectators;

	void clearextinfo()
	{
		lastinfo_ = -20000;
		lastinforesp_ = -20000;
		loopv(extgroups) delete[] extgroups[i]->team;
		extgroups.deletecontents();
		extgroups.setsize(0);
		extplayers.setsize(0);
	}

	bool extplayerresponse_(ucharbuf &p, ENetAddress &addr, int len)
	{
		int resp = getint(p);
		int type = getint(p);
		if (resp == 1)
		{
			extnumplayers = type;
			int nattr = getint(p);
			int prot = getint(p);
			if (prot != PROTOCOL_VERSION) return true;
			extgamemode = getint(p);
			extgamelimit = (getint(p)*1000)+lastmillis;
			extmaxplayers = getint(p);
			extmastermode = getint(p);
			if (nattr == 7)
			{
				extpaused = getint(p);
				extgamespeed = getint(p);
			}
			else
			{
				extpaused = 0;
				extgamespeed = 100;
			}
			getstring(extmap, p);
			getstring(extdesc, p);
		}
		else if (type == EXT_TEAMSCORE)
		{
			int ack = getint(p);
			int ver = getint(p);
			int iserr = getint(p);
			if (ack != EXT_ACK || ver != EXT_VERSION || iserr != EXT_NO_ERROR) return true;

			getint(p); getint(p);
			string team;
			getstring(team, p);
			while (!p.overread())
			{
				bool found = false;
				int score = getint(p);
				loopv(extgroups) if (extgroups[i]->team && team && !strcmp(extgroups[i]->team, team))
				{
					extgroups[i]->score = score;
					found = true;
				}
				if (!found)
				{
					extscoregroup *eg = extgroups.add(new extscoregroup);
					eg->team = newstring(team);
					eg->score = score;
				}
				int bases = getint(p);
				if (bases>=0) loopi(bases) getint(p);
				getstring(team, p);
			}
		}
		else if (type == EXT_PLAYERSTATS)
		{
			getint(p);
			int ack = getint(p);
			int ver = getint(p);
			int iserr = getint(p);
			if (ack != EXT_ACK || ver != EXT_VERSION || iserr != EXT_NO_ERROR) return true;

			int extresp = getint(p);
			if (extresp == EXT_PLAYERSTATS_RESP_STATS)
			{
				int cn = getint(p);
				extplayer_ *o = NULL;
				bool found = false;
				loopv(extplayers) if (extplayers[i].cn == cn)
				{
					o = &extplayers[i];
					found = true;
				}
				if (!found) o = &extplayers.add();
				o->cn = cn;
				o->ping = getint(p);
				getstring(o->name, p);
				getstring(o->team, p);
				o->frags = getint(p);
				o->flags = getint(p);
				o->deaths = getint(p);
				o->kpd = (float)o->frags/max(1, o->deaths);
				o->tks = getint(p);
				o->acc = getint(p);
				getint(p);
				getint(p);
				getint(p);
				o->privilege = getint(p);
				o->state = getint(p);
				p.get((uchar*)&o->ip, 3);
				addwhoisentry(o->ip, o->name);
				o->lastseen = lastmillis;
			}
			else if (extresp == EXT_PLAYERSTATS_RESP_IDS)
			{
				for (;;)
				{
					int cn = getint(p);
					if (p.overread()) break;
					loopv(extplayers) if (extplayers[i].cn == cn) extplayers[i].lastseen = lastmillis;
				}
				loopv(extplayers) if (extplayers[i].lastseen < lastmillis) extplayers.remove(i--);
				lastinforesp_ = lastmillis;
			}
		}
		return true;
	}

	static inline bool extplayersort(const extplayer_ *a, const extplayer_ *b)
    {
        if(a->state==CS_SPECTATOR)
        {
            if(b->state==CS_SPECTATOR) return strcmp(a->name, b->name) < 0;
            else return false;
        }
        else if(b->state==CS_SPECTATOR) return true;
        if(m_ctf || m_collect)
        {
            if(a->flags > b->flags) return true;
            if(a->flags < b->flags) return false;
        }
        if(a->frags > b->frags) return true;
        if(a->frags < b->frags) return false;
        return strcmp(a->name, b->name) < 0;
    }

    static inline bool extscoregroupcmp(const extscoregroup *x, const extscoregroup *y)
    {
        if(!x->team)
        {
            if(y->team) return false;
        }
        else if(!y->team) return true;
        if(x->score > y->score) return true;
        if(x->score < y->score) return false;
        if(x->players.length() > y->players.length()) return true;
        if(x->players.length() < y->players.length()) return false;
        return x->team && y->team && strcmp(x->team, y->team) < 0;
    }

    static int extgroupplayers()
    {
        int numgroups = 0;
        extspectators.setsize(0);
        loopv(extplayers)
        {
            extplayer_ *o = &extplayers[i];
            if(!showconnecting && !o->name[0]) continue;
            if(o->state==CS_SPECTATOR) { extspectators.add(o); continue; }
            const char *team = m_check(extgamemode, M_TEAM) && o->team[0] ? o->team : NULL;
            bool found = false;
            loopj(numgroups)
            {
                extscoregroup &g = *extgroups[j];
                if(team!=g.team && (!team || !g.team || strcmp(team, g.team))) continue;
                g.players.add(o);
                found = true;
            }
            if(found) continue;
            if(numgroups>=extgroups.length()) extgroups.add(new extscoregroup);
            extscoregroup &g = *extgroups[numgroups++];
            g.team = team? newstring(team): NULL;
            if(!team) g.score = 0;
            g.players.setsize(0);
            g.players.add(o);
        }
        loopi(numgroups) extgroups[i]->players.sort(extplayersort);
        extspectators.sort(extplayersort);
        //extgroups.sort(extscoregroupcmp, 0, numgroups);
        return numgroups;
    }

	bool extduplicatename(int cn, char *name)
    {
        loopv(extplayers) if(extplayers[i].cn!=cn && !strcmp(name, extplayers[i].name)) return true;
        return false;
    }

    const char *extcolorname(int cn, char *name = NULL)
    {
        if(name[0] && !extduplicatename(cn, name)) return name;
        static string cname[3];
        static int cidx = 0;
        cidx = (cidx+1)%3;
        formatstring(cname[cidx])("%s \fs\f5(%d)\fr", name, cn);
        return cname[cidx];
    }

	ENetAddress extaddress;

    void renderextscoreboard(g3d_gui &g)
    {
		extaddress.host = serverpreviewhost;
		extaddress.port = serverpreviewport;
		if (lastmillis-lastinfo_ > 5000)
		{
			if (extaddress.host)
			{
				uchar ping[MAXTRANS];
				ucharbuf p(ping, sizeof(ping));
				putint(p, 1);
				updateextinfo(extinfosock_, extaddress.host, server::serverinfoport(extaddress.port), p);

				ucharbuf q(ping, sizeof(ping));
				putint(q, 0);
				putint(q, EXT_TEAMSCORE);
				updateextinfo(extinfosock_, extaddress.host, server::serverinfoport(extaddress.port), q);

				ucharbuf r(ping, sizeof(ping));
				putint(r, 0);
				putint(r, EXT_PLAYERSTATS);
				putint(r, -1);
				updateextinfo(extinfosock_, extaddress.host, server::serverinfoport(extaddress.port), r);
				lastinfo_ = lastmillis;
			}
		}
		checkextinfo(extinfosock_, extplayerresponse_);

		if (lastmillis-lastinforesp_ > 20000 || extaddress.host==0) return;

        if(showservinfo)
        {
            string hostname;
            if(enet_address_get_host_ip(&extaddress, hostname, sizeof(hostname)) >= 0)
            {
                if(extservinfo
					[0]) g.titlef("%.25s", 0xFFFF80, NULL, extservinfo);
                else g.titlef("%s:%d", 0xFFFF80, NULL, hostname, extaddress.port);
            }
        }
     
        g.pushlist();
        g.spring();
        g.text(server::modename(extgamemode), 0xFFFF80);
        g.separator();
        const char *mname = extmap;
        g.text(mname[0] ? mname : "[new map]", 0xFFFF80);
        if(extgamespeed != 100) { g.separator(); g.textf("%d.%02dx", 0xFFFF80, NULL, extgamespeed/100, extgamespeed%100); }
        if(m_checknot(extgamemode, M_DEMO|M_EDIT|M_LOCAL) && mname[0] && extgamelimit > 0)
        {
            g.separator();
            if(extgamelimit <= lastmillis) g.text("intermission", 0xFFFF80);
            else 
            {
				int secs = max(extgamelimit-lastmillis, 0)/1000, mins = secs/60;
                secs %= 60;
                g.pushlist();
                g.strut(mins >= 10 ? 4.5f : 3.5f);
                g.textf("%d:%02d", 0xFFFF80, NULL, mins, secs);
                g.poplist();
            }
        }
        if(extpaused) { g.separator(); g.text("paused", 0xFFFF80); }
        g.spring();
        g.poplist();

        g.separator();
 
        int numgroups = extgroupplayers();
        loopk(numgroups)
        {
            if((k%2)==0) g.pushlist(); // horizontal
            
            extscoregroup &sg = *extgroups[k];
            int bgcolor = sg.team && m_check(extgamemode, M_TEAM) ? (!strcmp("good", sg.team) ? 0x3030C0 : 0xC03030) : 0,
                fgcolor = 0xFFFF80;

            g.pushlist(); // vertical
            g.pushlist(); // horizontal

            #define loopextscoregroup(o, b) \
                loopv(sg.players) \
                { \
                    extplayer_ *o = sg.players[i]; \
                    b; \
                }    

            g.pushlist();
            if(sg.team && m_check(extgamemode, M_TEAM))
            {
                g.pushlist();
                g.background(bgcolor, numgroups>1 ? 3 : 5);
                g.strut(1);
                g.poplist();
            }
            g.text("", 0, " ");
            g.poplist();

            if(sg.team && m_check(extgamemode, M_TEAM))
            {
                g.pushlist(); // vertical

                if(sg.score>=10000) g.textf("%s: WIN", fgcolor, NULL, sg.team);
                else g.textf("%s: %d", fgcolor, NULL, sg.team, sg.score);

                g.pushlist(); // horizontal
            }

            if(scoreboardfrags)
            {
				g.pushlist();
				g.strut(6);
				g.text("frags", fgcolor);
				loopextscoregroup(o,
				{
					if (scoreboardflags && o->flags) g.textf("%d-%d", 0xFFFFDD, NULL, o->frags, o->flags);
					else g.textf("%d", 0xFFFFDD, NULL, o->frags);
				});
				g.poplist();
            }

			if(scoreboarddeaths)
			{
				g.pushlist();
				g.strut(6);
				g.text("deaths", fgcolor);
				loopextscoregroup(o, g.textf("%d", 0xFFFFDD, NULL, o->deaths));
				g.poplist();
			}

			if(scoreboardaccuracy)
			{
				g.pushlist();
				g.strut(6);
				g.text("acc", fgcolor);
				loopextscoregroup(o, g.textf("%d%%", 0xFFFFDD, NULL, o->acc));
				g.poplist();
			}

			if(scoreboardkpd)
			{
				g.pushlist();
				g.strut(6);
				g.text("kpd", fgcolor);
				loopextscoregroup(o, g.textf("%.2f", 0xFFFFDD, NULL, (float)o->kpd));
				g.poplist();
			}

			if(scoreboardteamkills)
			{
				g.pushlist();
				g.strut(6);
				g.text("tks", fgcolor);
				loopextscoregroup(o, g.textf("%d", 0xFFFFDD, NULL, o->tks));
				g.poplist();
			}

            g.pushlist();
            g.text("name", fgcolor);
            g.strut(13);
            loopextscoregroup(o, 
            {
                int status = o->state!=CS_DEAD ? 0xFFFFDD : 0x606060;
                if(o->privilege)
                {
                    status = (o->privilege>=PRIV_ADMIN) ? 0xFF8000 : (o->privilege==PRIV_AUTH)? 0x9040FF: 0x40FF80;
                    if(o->state==CS_DEAD) status = (status>>1)&0x7F7F7F;
                }
                g.textf("%s ", status, NULL, extcolorname(o->cn, o->name));
            });
            g.poplist();

            if(showping)
            {
                g.pushlist();
                g.text("ping", fgcolor);
                g.strut(6);
                loopextscoregroup(o,
                {
                    if(o->state==CS_LAGGED) g.text("LAG", 0xFFFFDD);
                    else g.textf("%d", 0xFFFFDD, NULL, o->ping);
                });
                g.poplist();
            }

            if(showclientnum)
            {
                g.pushlist();
				g.strut(3);
                g.text("cn", fgcolor);
                loopextscoregroup(o, g.textf("%d", 0xFFFFDD, NULL, o->cn));
                g.poplist();
            }

			if(scoreboardcountry)
            {
				g.pushlist();
				g.strut(6);
				g.text("country", fgcolor);
				loopextscoregroup(o,
								{
									const char icon[MAXSTRLEN] = "";
									const char *countrycode = GeoIP_country_code_by_ipnum(geoip, endianswap(o->ip));
									const char *country = (scoreboardcountry&2)? countrycode:
														  (scoreboardcountry&4)? GeoIP_country_name_by_ipnum(geoip, endianswap(o->ip)): "";
									if (scoreboardcountry&1) formatstring(icon)("%s.png", countrycode);
									g.textf("%s", 0xFFFFDD, (scoreboardcountry&1)? icon: NULL, country);
								}
							  );
                g.space(1);
				g.poplist();
            }

            if(sg.team && m_check(extgamemode, M_TEAM))
            {
                g.poplist(); // horizontal
                g.poplist(); // vertical
            }

            g.poplist(); // horizontal
            g.poplist(); // vertical

            if(k+1<numgroups && (k+1)%2) g.space(2);
            else g.poplist(); // horizontal
        }
        
        if(showspectators && extspectators.length())
        {
            g.pushlist();
                
            g.pushlist();
            g.text("spectator", 0xFFFF80, " ");
            loopv(extspectators) 
            {
                extplayer_ *o = extspectators[i];
                int status = 0xFFFFDD;
                if(o->privilege) status = (o->privilege>=PRIV_ADMIN) ? 0xFF8000 : (o->privilege==PRIV_AUTH)? 0x9040FF: 0x40FF80;
                g.text(extcolorname(o->cn, o->name), status, "spectator");
            }
            g.poplist();

			if (scoreboardspecping)
			{
				g.space(1);
				g.pushlist();
				g.strut(3);
				g.text("ping", 0xFFFF80);
				loopv(extspectators) g.textf("%d", 0xFFFFDD, NULL, extspectators[i]->ping);
				g.poplist();
			}

            if(showclientnum)
            {
                g.space(1);
                g.pushlist();
                g.strut(3);
				g.text("cn", 0xFFFF80);
                loopv(extspectators) g.textf("%d", 0xFFFFDD, NULL, extspectators[i]->cn);
                g.poplist();
            }

			if(scoreboardcountry)
			{
				g.pushlist();
				g.text("country", 0xFFFF80);
				loopv(extspectators)
								{
									const char icon[MAXSTRLEN] = "";
									const char *countrycode = GeoIP_country_code_by_ipnum(geoip, endianswap(extspectators[i]->ip));
									const char *country = (scoreboardcountry&2)? countrycode:
															(scoreboardcountry&4)? GeoIP_country_name_by_ipnum(geoip, endianswap(extspectators[i]->ip)): "";
									if (scoreboardcountry&1) formatstring(icon)("%s.png", countrycode);
									g.textf("%s ", 0xFFFFDD, (scoreboardcountry&1)? icon: NULL, country);
								}
				//g.space(1);
				g.poplist();
			}

            g.poplist();
        }
    }

	VARP(whoisenabled, 0, 1, 1);
	struct whoisent { uint ip; vector<char *> names; };
	vector<whoisent> wies;

	ICOMMAND(whoisentry, "ss", (const char *ip, const char *names),
	{
		if (!names) return;
		whoisent wie;
		wie.ip = endianswap(GeoIP_addr_to_num(ip));
		wie.ip = wie.ip&0xFFFFFF;
		splitlist(names, wie.names);
		wies.add(wie);
	});

	vector<char *> *playernames(int cn)
	{
		fpsent *cl = getclient(cn);
		if (!cl) return NULL;
		int nip = cl->ip&0xFFFFFF;
		for (int i = 0; i < wies.length(); i++)
		{
			if (wies[i].ip == nip)
			{
				return &wies[i].names;
			}
		}
		return NULL;
	}

	void addwhoisentry(uint ip, const char *name)
	{
		if (!scoreboardextinfo || !whoisenabled) return;
		int nip = ip&0xFFFFFF; //%(256*256*256)
		bool found = false;
		for (int i = 0; i < wies.length(); i++) if (wies[i].ip == nip)
		{
			for (int j = 0; j < wies[i].names.length(); j++)
			{
				if (!strcmp(wies[i].names[j], name))
				{
					wies[i].names.remove(j);
					break;
				}
			}
			wies[i].names.insert(0, newstring(name));
			while (wies[i].names.length() > 15) delete[] wies[i].names.pop();
			found = true;
		}
		if (!found)
		{
			whoisent &wie = wies.add();
			wie.ip = nip;
			wie.names.add(newstring(name));
		}
	}

	void addplayerwhois(int cn)
	{
		fpsent *cl = getclient(cn);
		if (!cl || !cl->ip) return;
		addwhoisentry(cl->ip, cl->name);
	}

	VARHSC(globalwhois, 0, 1, 1);

	void whoisip(uint ip, const char *name)
	{
		if (!scoreboardextinfo) { conoutf("\f3error: whois requires extinfo to be enabled"); return; }
		if (!whoisenabled) { conoutf("\f3error: whois is disabled"); return; }
		ip = ip&0xFFFFFF;
		if (!ip) return;
		for (int i = 0; i < wies.length(); i++)
		{
			if (wies[i].ip == ip)
			{
				defformatstring(line)("\f0possible names for \f1%s\f0 are: \f3", name);
				loopvj(wies[i].names)
				{
					concatstring(line, wies[i].names[j]);
					concatstring(line, " ");
				}
				conoutf(line);
			}
		}
	}

	void whois(const int cn, bool msg)
	{
		if (!globalwhois && !msg) return;
		if (!scoreboardextinfo) { if (msg) conoutf("\f3error: whois requires extinfo to be enabled"); return; }
		if (!whoisenabled) { if (msg) conoutf("\f3error: whois is disabled"); return; }
		fpsent *cl = getclient(cn);
		if (!cl || !cl->ip) return;
		int nip = cl->ip&0xFFFFFF;
		if (!nip) return;
		for (int i = 0; i < wies.length(); i++)
		{
			if (wies[i].ip == nip)
			{
				defformatstring(line)("\f0possible names for \f1%s\f0 are: \f3", cl->name);
				loopvj(wies[i].names)
				{
					concatstring(line, wies[i].names[j]);
					concatstring(line, " ");
				}
				conoutf(line);
			}
		}
	}
	ICOMMAND(whois, "i", (const int *cn, bool msg), { whois(*cn, true); });

	void whoisname(const char *name, int exact)
	{
		if (!scoreboardextinfo) { conoutf("\f3error: whois requires extinfo to be enabled"); return; }
		if (!whoisenabled) { conoutf("\f3error: whois is disabled"); return; }
		if (!name || !name[0]) return;
		conoutf("\f0everyone who used the name \f1%s\f0:\n\f6============", name);
		for (int i = 0; i < wies.length(); i++)
		{
			bool found = false;
			string cname, bname;
			if (!exact)
			{
				strcpy(cname, name);
				strlwr(cname);
			}
			loopvj(wies[i].names)
			{
				if (!exact)
				{
					strcpy(bname, wies[i].names[j]);
					strlwr(bname);
					if (strstr(bname, cname)) found = true;
				}
				else if (strcasecmp(wies[i].names[j], name) == 0) found = true;
			}

			if (found)
			{
				defformatstring(line)("\f0%d.%d.%d.* \f1%s\f0: \f3", wies[i].ip&0xFF, (wies[i].ip&0xFF00)>>8, (wies[i].ip&0xFF0000)>>16, GeoIP_country_name_by_ipnum(geoip, endianswap(wies[i].ip)));
				loopvj(wies[i].names)
				{
					concatstring(line, wies[i].names[j]);
					concatstring(line, " ");
				}
				conoutf("%s", line);
			}
		}
		conoutf("\f6============");
	}
	ICOMMAND(whoisname, "si", (const char *name, const int *exact), { whoisname(name, *exact); });


	void loadwhoisdb()
	{
		execfile("whois.cfg", false);
	}

	void saveclearwhoisdb()
	{
		stream *f = openutf8file(path("whois.cfg", true), "w");
		loopv(wies)
		{
			f->printf("whoisentry %s ", GeoIP_num_to_addr(endianswap(wies[i].ip)));
			char text[MAXTRANS];
			text[0] = '\0';
			for (int j = 0; j < wies[i].names.length(); j++)
			{
				concatstring(text, wies[i].names[j]);
				if (j != wies[i].names.length()-1) concatstring(text, " ");
			}
			f->putstring(escapestring(text));
			f->printf("\n");
			wies[i].names.deletearrays();
		}
		delete f;
		wies.setsize(0);
	}
}

